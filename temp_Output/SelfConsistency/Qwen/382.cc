#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaUdpSimulation");

int main(int argc, char *argv[]) {
    // Enable logging for UdpServer and UdpClient
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Create CSMA channel
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    // Install devices
    NetDeviceContainer devices = csma.Install(nodes);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up UDP server on Node 3 (index 2)
    UdpServerHelper server(9); // Port 9
    ApplicationContainer serverApps = server.Install(nodes.Get(2));
    serverApps.Start(Seconds(0));
    serverApps.Stop(Seconds(10));

    // Set up UDP client on Node 1 (index 0)
    UdpClientHelper client(interfaces.GetAddress(2), 9); // Server IP and port
    client.SetAttribute("MaxPackets", UintegerValue(4294967295)); // Unlimited packets
    client.SetAttribute("Interval", TimeValue(MilliSeconds(100))); // Packet interval
    client.SetAttribute("PacketSize", UintegerValue(1024)); // Packet size

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2));
    clientApps.Stop(Seconds(10));

    // Set up PCAP tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("csma-udp-simulation.tr");
    csma.EnableAsciiAll(stream);

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}