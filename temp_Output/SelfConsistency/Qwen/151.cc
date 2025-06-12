#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("BusTopologySimulation");

int main(int argc, char *argv[]) {
    // Enable logging for UdpEchoClient and Server
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 3 nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Create CSMA channel
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(100e6)));
    csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(6560)));

    // Install the CSMA channel on all nodes
    NetDeviceContainer devices = csma.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup applications
    uint16_t port = 9; // Use standard echo port

    // Create and install UDP Echo server on node 2
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Create UDP Echo client on node 0 to send packets to node 2 (IP: 10.1.1.3)
    UdpEchoClientHelper client(interfaces.GetAddress(2), port);
    client.SetAttribute("MaxPackets", UintegerValue(5));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable PCAP tracing
    csma.EnablePcapAll("bus_topology_simulation");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}