#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/distance-vector-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(3);

    // Point-to-Point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // NetDevice containers for two links: A-B and B-C
    NetDeviceContainer devAB = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devBC = p2p.Install(nodes.Get(1), nodes.Get(2));

    // Install Internet stack with Distance Vector Routing (with Split Horizon)
    DistanceVectorRoutingHelper dvHelper;
    dvHelper.Set("SplitHorizon", BooleanValue(true));

    InternetStackHelper internet;
    internet.SetRoutingHelper(dvHelper);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifAB = ipv4.Assign(devAB);

    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ifBC = ipv4.Assign(devBC);

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Simple application: Node A sends UDP packets to Node C
    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(ifBC.GetAddress(1), sinkPort));
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(2));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
    onoff.SetAttribute("DataRate", StringValue("1Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(9.0)));
    ApplicationContainer app = onoff.Install(nodes.Get(0));

    // Schedule routing table prints to demonstrate Split Horizon effect
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
        Simulator::Schedule(Seconds(2.5), &Ipv4::PrintRoutingTable, ipv4, Create<OutputStreamWrapper>(&std::cout));
        Simulator::Schedule(Seconds(5.0), &Ipv4::PrintRoutingTable, ipv4, Create<OutputStreamWrapper>(&std::cout));
        Simulator::Schedule(Seconds(8.0), &Ipv4::PrintRoutingTable, ipv4, Create<OutputStreamWrapper>(&std::cout));
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}