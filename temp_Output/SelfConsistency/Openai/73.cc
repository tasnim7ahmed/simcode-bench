#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create three nodes: A, B, C
    NodeContainer routers;
    routers.Create(3);

    // Create point-to-point links: A-B and B-C
    NodeContainer ab = NodeContainer(routers.Get(0), routers.Get(1)); // A-B
    NodeContainer bc = NodeContainer(routers.Get(1), routers.Get(2)); // B-C

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devAB = p2p.Install(ab);
    NetDeviceContainer devBC = p2p.Install(bc);

    // Install the internet stack
    InternetStackHelper internet;
    internet.Install(routers);

    // Assign IP addresses:
    // A's loopback: 10.0.0.1/32
    // B-A net: 192.168.1.0/30
    // B-C net: 192.168.2.0/30
    // C's loopback: 10.0.1.1/32

    // Assign to A-B link
    Ipv4AddressHelper ipv4_ab;
    ipv4_ab.SetBase("192.168.1.0", "255.255.255.252");
    Ipv4InterfaceContainer if_ab = ipv4_ab.Assign(devAB);

    // Assign to B-C link
    Ipv4AddressHelper ipv4_bc;
    ipv4_bc.SetBase("192.168.2.0", "255.255.255.252");
    Ipv4InterfaceContainer if_bc = ipv4_bc.Assign(devBC);

    // Assign loopback to A and C manually
    Ptr<Ipv4> ipv4A = routers.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4C = routers.Get(2)->GetObject<Ipv4>();
    int32_t loIfA = ipv4A->GetInterfaceForDevice(ipv4A->GetNetDevice(0));
    int32_t loIfC = ipv4C->GetInterfaceForDevice(ipv4C->GetNetDevice(0));
    ipv4A->AddAddress(loIfA, Ipv4InterfaceAddress("10.0.0.1", "255.255.255.255"));
    ipv4C->AddAddress(loIfC, Ipv4InterfaceAddress("10.0.1.1", "255.255.255.255"));

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up OnOff UDP application on Router A (send to Router C)
    uint16_t sinkPort = 9999;
    Address sinkAddress(InetSocketAddress(Ipv4Address("10.0.1.1"), sinkPort));
    OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
    onoff.SetAttribute("DataRate", StringValue("500Kbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(9.0)));

    ApplicationContainer app = onoff.Install(routers.Get(0)); // Router A

    // Set up PacketSink application on Router C
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = sink.Install(routers.Get(2)); // Router C
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Enable pcap tracing
    p2p.EnablePcapAll("three-router");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}