#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(7);

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevices02 = p2p.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer p2pDevices12 = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer p2pDevices56 = p2p.Install(nodes.Get(5), nodes.Get(6));

    // Create CSMA/CD link
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("10Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("2ms"));

    NodeContainer csmaNodes;
    csmaNodes.Add(nodes.Get(2));
    csmaNodes.Add(nodes.Get(3));
    csmaNodes.Add(nodes.Get(4));
    csmaNodes.Add(nodes.Get(5));
    NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces02 = address.Assign(p2pDevices02);
    Ipv4InterfaceContainer p2pInterfaces12 = address.Assign(p2pDevices12);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces56 = address.Assign(p2pDevices56);

    // Configure routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create UDP server on node 6
    UdpServerHelper server(9);
    ApplicationContainer serverApp = server.Install(nodes.Get(6));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Create UDP client on node 0
    uint32_t packetSize = 210;
    DataRate dataRate("448Kbps");
    UdpClientHelper client(p2pInterfaces56.GetAddress(1), 9);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(packetSize * 8.0 / dataRate.GetBitRate())));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable PCAP tracing
    p2p.EnablePcapAll("mixed_network");
    csma.EnablePcapAll("mixed_network");

    // Enable queue monitoring
    for (int i = 0; i < (int)csmaDevices.GetN(); ++i) {
        csma.EnableAsciiAll("mixed_network-csma", csmaDevices.Get(i));
    }

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}