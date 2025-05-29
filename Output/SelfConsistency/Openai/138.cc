#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoExample");

void
RxTrace(Ptr<const Packet> packet, const Address &address)
{
    NS_LOG_INFO("Packet received at " << Simulator::Now().GetSeconds() << "s, Size: " << packet->GetSize() << " bytes");
}

void
TxTrace(Ptr<const Packet> packet)
{
    NS_LOG_INFO("Packet sent at " << Simulator::Now().GetSeconds() << "s, Size: " << packet->GetSize() << " bytes");
}

int
main(int argc, char *argv[])
{
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoExample", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect nodes: 0-1, 1-2, 2-3
    NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
    NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));
    NodeContainer n2n3 = NodeContainer(nodes.Get(2), nodes.Get(3));

    NetDeviceContainer d0d1 = p2p.Install(n0n1);
    NetDeviceContainer d1d2 = p2p.Install(n1n2);
    NetDeviceContainer d2d3 = p2p.Install(n2n3);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = ipv4.Assign(d0d1);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = ipv4.Assign(d1d2);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i3 = ipv4.Assign(d2d3);

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install UDP Echo Server on Node 3
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Install UDP Echo Client on Node 0, target is last interface on Node 3
    UdpEchoClientHelper echoClient(i2i3.GetAddress(1), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Packet tracing for transmissions and receptions
    Config::ConnectWithoutContext(
        "/NodeList/0/ApplicationList/0/$ns3::UdpEchoClient/Rx", MakeCallback(&RxTrace));
    Config::ConnectWithoutContext(
        "/NodeList/0/ApplicationList/0/$ns3::UdpEchoClient/Tx", MakeCallback(&TxTrace));
    Config::ConnectWithoutContext(
        "/NodeList/3/ApplicationList/0/$ns3::UdpEchoServer/Rx", MakeCallback(&RxTrace));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}