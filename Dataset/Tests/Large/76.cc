#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

void TestNodeCreation()
{
    NodeContainer c;
    c.Create(7);
    NS_ASSERT_MSG(c.GetN() == 7, "Node creation failed");
}

void TestPointToPointLinks()
{
    NodeContainer n0n2, n1n2, n5n6;
    n0n2.Create(2);
    n1n2.Create(2);
    n5n6.Create(2);
    
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer d0d2 = p2p.Install(n0n2);
    NetDeviceContainer d1d2 = p2p.Install(n1n2);
    
    NS_ASSERT_MSG(d0d2.GetN() == 2, "Point-to-point link setup failed");
    NS_ASSERT_MSG(d1d2.GetN() == 2, "Point-to-point link setup failed");
}

void TestCsmaLinks()
{
    NodeContainer n2345;
    n2345.Create(4);
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer d2345 = csma.Install(n2345);
    NS_ASSERT_MSG(d2345.GetN() == 4, "CSMA link setup failed");
}

void TestIpAssignment()
{
    NodeContainer nodes;
    nodes.Create(2);
    InternetStackHelper internet;
    internet.Install(nodes);
    PointToPointHelper p2p;
    NetDeviceContainer devices = p2p.Install(nodes);
    
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);
    
    NS_ASSERT_MSG(interfaces.GetN() == 2, "IP Address assignment failed");
}

void TestOnOffApplication()
{
    NodeContainer nodes;
    nodes.Create(2);
    InternetStackHelper internet;
    internet.Install(nodes);
    PointToPointHelper p2p;
    NetDeviceContainer devices = p2p.Install(nodes);
    
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);
    
    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    onoff.SetConstantRate(DataRate("300bps"));
    onoff.SetAttribute("PacketSize", UintegerValue(50));
    
    ApplicationContainer apps = onoff.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));
    
    NS_ASSERT_MSG(!apps.Get(0)->GetStartTime().IsZero(), "OnOff Application did not start");
}

int main()
{
    TestNodeCreation();
    TestPointToPointLinks();
    TestCsmaLinks();
    TestIpAssignment();
    TestOnOffApplication();
    
    std::cout << "All tests passed!" << std::endl;
    return 0;
}
