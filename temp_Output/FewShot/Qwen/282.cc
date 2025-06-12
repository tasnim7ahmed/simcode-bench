#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("TcpServer", LOG_LEVEL_INFO);
    LogComponentEnable("TcpClient", LOG_LEVEL_INFO);

    // Create four nodes (Node 0 as server, Nodes 1-3 as clients)
    NodeContainer nodes;
    nodes.Create(4);

    // Configure CSMA link
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer devices = csma.Install(nodes);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up TCP Server on node 0
    uint16_t port = 8080;  // TCP port number
    TcpServerHelper tcpServer(port);
    ApplicationContainer serverApp = tcpServer.Install(nodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up TCP Clients on nodes 1, 2, and 3
    for (uint32_t i = 1; i <= 3; ++i) {
        TcpClientHelper tcpClient(interfaces.GetAddress(0), port);
        tcpClient.SetAttribute("MaxBytes", UintegerValue(0)); // infinite data
        tcpClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        tcpClient.SetAttribute("PacketSize", UintegerValue(1000));
        ApplicationContainer clientApp = tcpClient.Install(nodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
    }

    // Enable PCAP tracing
    AsciiTraceHelper asciiTraceHelper;
    point_to_point::PointToPointNetDevice::EnablePcapAll("tcp_csma_simulation");

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}