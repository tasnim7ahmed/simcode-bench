#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable packet tracing
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create routers
    NodeContainer routers;
    routers.Create(3); // A=0, B=1, C=2

    // Point-to-point helper
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Create links
    // A <-> B
    NodeContainer a_b = NodeContainer(routers.Get(0), routers.Get(1));
    NetDeviceContainer dev_ab = p2p.Install(a_b);

    // B <-> C
    NodeContainer b_c = NodeContainer(routers.Get(1), routers.Get(2));
    NetDeviceContainer dev_bc = p2p.Install(b_c);

    // Each router has a "loopback"/host interface
    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(routers);

    // Assign IP addresses
    Ipv4AddressHelper ip;

    // A-B subnet: x.x.x.0/30
    ip.SetBase("10.1.1.0", "255.255.255.252");
    Ipv4InterfaceContainer if_ab = ip.Assign(dev_ab);

    // B-C subnet: y.y.y.0/30
    ip.SetBase("10.2.2.0", "255.255.255.252");
    Ipv4InterfaceContainer if_bc = ip.Assign(dev_bc);

    // Set /32 addresses for router A and C via assigning to loopback interface
    Ptr<Ipv4> ipv4A = routers.Get(0)->GetObject<Ipv4>();
    int32_t ifA = ipv4A->AddInterface(CreateObject<LoopbackNetDevice>());
    ipv4A->AddAddress(ifA, Ipv4InterfaceAddress("192.168.1.1", "255.255.255.255"));
    ipv4A->SetUp(ifA);

    Ptr<Ipv4> ipv4C = routers.Get(2)->GetObject<Ipv4>();
    int32_t ifC = ipv4C->AddInterface(CreateObject<LoopbackNetDevice>());
    ipv4C->AddAddress(ifC, Ipv4InterfaceAddress("192.168.3.1", "255.255.255.255"));
    ipv4C->SetUp(ifC);

    // Enable global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up OnOff application on Router A -> send to Router C's /32
    uint16_t port = 50000;
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address("192.168.3.1"), port)));
    onoff.SetAttribute("DataRate", StringValue("5Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(9.0)));
    ApplicationContainer app = onoff.Install(routers.Get(0));

    // Set up PacketSink application on Router C
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(routers.Get(2));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Enable pcap tracing on p2p devices
    p2p.EnablePcapAll("three-router");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}