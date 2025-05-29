#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyExample");

void
TxTrace(Ptr<const Packet> packet)
{
    NS_LOG_INFO("Packet transmitted: " << packet->GetSize() << " bytes at " << Simulator::Now().GetSeconds() << "s");
}

void
RxTrace(Ptr<const Packet> packet, const Address &from)
{
    NS_LOG_INFO("Packet received: " << packet->GetSize() << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4() << " at " << Simulator::Now().GetSeconds() << "s");
}

int
main (int argc, char *argv[])
{
    // Enable logging for this script and key applications
    LogComponentEnable("StarTopologyExample", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes: 0 - central, 1 & 2 - peripherals
    NodeContainer nodes;
    nodes.Create(3);

    // Create point-to-point links: 0<->1, 0<->2
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    // Link between node 0 and node 1
    NodeContainer n0n1(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer dev0n1 = pointToPoint.Install(n0n1);

    // Link between node 0 and node 2
    NodeContainer n0n2(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer dev0n2 = pointToPoint.Install(n0n2);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer iface0n1 = address.Assign(dev0n1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer iface0n2 = address.Assign(dev0n2);

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install UDP server on node 2 (peripheral)
    uint16_t port = 4000;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Install UDP client on node 1 (peripheral), sends packets to node 2's IP
    UdpClientHelper client(iface0n2.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(5));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(nodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Logging callbacks
    Config::ConnectWithoutContext(
        "/NodeList/1/ApplicationList/0/$ns3::UdpClient/Tx", MakeCallback(&TxTrace));

    Config::ConnectWithoutContext(
        "/NodeList/2/ApplicationList/0/$ns3::UdpServer/Rx", MakeCallback(&RxTrace));

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}