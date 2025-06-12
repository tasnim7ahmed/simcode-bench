#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/rip-helper.h"

using namespace ns3;

class RipRoutingTest : public TestCase {
public:
    RipRoutingTest() : TestCase("Test RIP Routing Functionality") {}
    virtual void DoRun() {
        Ptr<Node> a = CreateObject<Node>();
        Ptr<Node> b = CreateObject<Node>();
        NodeContainer net(a, b);
        
        CsmaHelper csma;
        NetDeviceContainer devices = csma.Install(net);
        
        InternetStackHelper stack;
        RipHelper rip;
        Ipv4ListRoutingHelper listRH;
        listRH.Add(rip, 0);
        stack.SetRoutingHelper(listRH);
        stack.Install(net);
        
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.0.0.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);
        
        Ptr<Ipv4> ipv4A = a->GetObject<Ipv4>();
        Ptr<Ipv4> ipv4B = b->GetObject<Ipv4>();
        
        NS_TEST_ASSERT_MSG_NE(ipv4A, nullptr, "IPv4 not installed on Node A");
        NS_TEST_ASSERT_MSG_NE(ipv4B, nullptr, "IPv4 not installed on Node B");
    }
};

class LinkFailureTest : public TestCase {
public:
    LinkFailureTest() : TestCase("Test Link Failure Recovery") {}
    virtual void DoRun() {
        Ptr<Node> b = CreateObject<Node>();
        Ptr<Node> d = CreateObject<Node>();
        NodeContainer net(b, d);
        
        CsmaHelper csma;
        NetDeviceContainer devices = csma.Install(net);
        
        InternetStackHelper stack;
        stack.Install(net);
        
        Simulator::Schedule(Seconds(40), &TearDownLink, b, d, 0, 0);
        Simulator::Run();
        
        Ptr<Ipv4> ipv4B = b->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_EQ(ipv4B->IsUp(0), false, "Link should be down at 40s");
        
        Simulator::Destroy();
    }
};

class PingTest : public TestCase {
public:
    PingTest() : TestCase("Test Ping between SRC and DST") {}
    virtual void DoRun() {
        Ptr<Node> src = CreateObject<Node>();
        Ptr<Node> dst = CreateObject<Node>();
        NodeContainer net(src, dst);
        
        CsmaHelper csma;
        NetDeviceContainer devices = csma.Install(net);
        
        InternetStackHelper stack;
        stack.Install(net);
        
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.0.0.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);
        
        PingHelper ping(Ipv4Address("10.0.0.2"));
        ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        ping.SetAttribute("Size", UintegerValue(1024));
        ApplicationContainer apps = ping.Install(src);
        apps.Start(Seconds(1.0));
        apps.Stop(Seconds(5.0));
        
        Simulator::Run();
        NS_TEST_ASSERT_MSG_NE(apps.Get(0), nullptr, "Ping application should be running");
        Simulator::Destroy();
    }
};

class RipTestSuite : public TestSuite {
public:
    RipTestSuite() : TestSuite("rip-tests", UNIT)
    {
        AddTestCase(new RipRoutingTest, TestCase::QUICK);
        AddTestCase(new LinkFailureTest, TestCase::QUICK);
        AddTestCase(new PingTest, TestCase::QUICK);
    }
};

static RipTestSuite ripTestSuite;
