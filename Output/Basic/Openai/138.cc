#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoFourNodeExample");

void
TxTrace(Ptr<const Packet> packet)
{
    NS_LOG_UNCOND("Packet sent at " << Simulator::Now().GetSeconds() << "s, Size: " << packet->GetSize());
}

void
RxTrace(Ptr<const Packet> packet, const Address &address)
{
    NS_LOG_UNCOND("Packet received at " << Simulator::Now().GetSeconds() << "s, Size: " << packet->GetSize());
}

int
main(int argc, char *argv[])
{
    LogComponentEnable("UdpEchoFourNodeExample", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices01 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices12 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer devices23 = pointToPoint.Install(nodes.Get(2), nodes.Get(3));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces01 = address.Assign(devices01);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces23 = address.Assign(devices23);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);

    ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces23.GetAddress(1), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));
    
    Config::ConnectWithoutContext("/NodeList/0/ApplicationList/0/$ns3::UdpEchoClient/Tx", MakeCallback(&TxTrace));
    Config::ConnectWithoutContext("/NodeList/3/ApplicationList/0/$ns3::UdpEchoServer/Rx", MakeCallback(&RxTrace));

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}