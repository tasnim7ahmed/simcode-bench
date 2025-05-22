#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <cassert>

using namespace ns3;

void TestNetworkTopology()
{
    NodeContainer c;
    c.Create(4);
    assert(c.GetN() == 4 && "Failed to create nodes");
}

void TestIpAddressAssignment()
{
    NodeContainer c;
    c.Create(4);
    InternetStackHelper internet;
    internet.Install(c);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    PointToPointHelper p2p;
    NetDeviceContainer devices = p2p.Install(c.Get(0), c.Get(1));
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    assert(interfaces.GetAddress(0) != Ipv4Address("0.0.0.0") && "IP address assignment failed");
}

void TestRoutingSetup()
{
    NodeContainer c;
    c.Create(4);
    InternetStackHelper internet;
    internet.Install(c);
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    Ptr<Ipv4> ipv4 = c.Get(1)->GetObject<Ipv4>();
    assert(ipv4 && "Routing table setup failed");
}

void TestTrafficFlow()
{
    NodeContainer c;
    c.Create(4);
    InternetStackHelper internet;
    internet.Install(c);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    PointToPointHelper p2p;
    NetDeviceContainer devices = p2p.Install(c.Get(0), c.Get(1));
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), port)));
    ApplicationContainer apps = onoff.Install(c.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
    apps = sink.Install(c.Get(1));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();
}

int main()
{
    TestNetworkTopology();
    TestIpAddressAssignment();
    TestRoutingSetup();
    TestTrafficFlow();
    std::cout << "All tests passed!" << std::endl;
    return 0;
}
