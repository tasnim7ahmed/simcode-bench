#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

void TxTrace(Ptr<const Packet> packet)
{
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: Packet transmitted, size: " << packet->GetSize());
}

void RxTrace(Ptr<const Packet> packet, const Address &address)
{
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: Packet received, size: " << packet->GetSize());
}

int main(int argc, char *argv[])
{
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d01 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d12 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer d23 = pointToPoint.Install(nodes.Get(2), nodes.Get(3));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer i01, i12, i23;

    address.SetBase("10.1.1.0", "255.255.255.0");
    i01 = address.Assign(d01);

    address.SetBase("10.1.2.0", "255.255.255.0");
    i12 = address.Assign(d12);

    address.SetBase("10.1.3.0", "255.255.255.0");
    i23 = address.Assign(d23);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(i23.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Ptr<UdpEchoClient> clientApp = DynamicCast<UdpEchoClient>(clientApps.Get(0));
    clientApp->TraceConnectWithoutContext("Tx", MakeCallback(&TxTrace));

    Ptr<UdpEchoServer> serverApp = DynamicCast<UdpEchoServer>(serverApps.Get(0));
    serverApp->TraceConnectWithoutContext("Rx", MakeCallback(&RxTrace));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}