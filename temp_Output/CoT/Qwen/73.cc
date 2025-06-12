#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeRouterUdpSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    NodeContainer routers;
    routers.Create(3);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer linkAB = p2p.Install(routers.Get(0), routers.Get(1));
    NetDeviceContainer linkBC = p2p.Install(routers.Get(1), routers.Get(2));

    InternetStackHelper stack;
    stack.Install(routers);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.252"); // A <-> B subnet
    address.Assign(linkAB);

    address.SetBase("10.1.2.0", "255.255.255.252"); // B <-> C subnet
    address.Assign(linkBC);

    // Assign /32 addresses to Router A and C on their respective interfaces
    Ptr<Ipv4> ipv4A = routers.Get(0)->GetObject<Ipv4>();
    uint32_t idxA = ipv4A->AddInterface(linkAB.Get(0));
    ipv4A->AddAddress(idxA, Ipv4InterfaceAddress(Ipv4Address("1.1.1.1"), Ipv4Mask("/32")));
    ipv4A->SetMetric(idxA, 1);
    ipv4A->SetUp(idxA);

    Ptr<Ipv4> ipv4C = routers.Get(2)->GetObject<Ipv4>();
    uint32_t idxC = ipv4C->AddInterface(linkBC.Get(1));
    ipv4C->AddAddress(idxC, Ipv4InterfaceAddress(Ipv4Address("3.3.3.3"), Ipv4Mask("/32")));
    ipv4C->SetMetric(idxC, 1);
    ipv4C->SetUp(idxC);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(routers.Get(2));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    OnOffHelper onoff("ns3::UdpSocketFactory", Address());
    onoff.SetAttribute("Remote", AddressValue(InetSocketAddress(Ipv4Address("3.3.3.3"), port)));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer app = onoff.Install(routers.Get(0));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("three-router-udp-simulation.tr"));

    p2p.EnablePcapAll("three-router-udp-simulation");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}