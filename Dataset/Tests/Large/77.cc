#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-list-routing.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ripng-helper.h"

using namespace ns3;

class RipNgTest : public TestCase
{
public:
    RipNgTest() : TestCase("Test RIPng Routing Setup") {}
    virtual void DoRun()
    {
        NodeContainer nodes;
        nodes.Create(6);

        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
        csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

        NetDeviceContainer devices = csma.Install(nodes);

        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv6AddressHelper ipv6;
        ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
        Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);

        // Check if each node has an IPv6 address assigned
        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            Ptr<Ipv6> ipv6 = nodes.Get(i)->GetObject<Ipv6>();
            NS_TEST_ASSERT_MSG_NE(ipv6->GetNInterfaces(), 0, "IPv6 interface is not assigned");
        }
    }
};

class RipNgRoutingTest : public TestCase
{
public:
    RipNgRoutingTest() : TestCase("Test RIPng Routing Tables") {}
    virtual void DoRun()
    {
        NodeContainer routers;
        routers.Create(4);

        RipNgHelper ripNgRouting;
        Ipv6ListRoutingHelper listRH;
        listRH.Add(ripNgRouting, 0);

        InternetStackHelper stack;
        stack.SetIpv4StackInstall(false);
        stack.SetRoutingHelper(listRH);
        stack.Install(routers);

        for (uint32_t i = 0; i < routers.GetN(); ++i)
        {
            Ptr<Ipv6> ipv6 = routers.Get(i)->GetObject<Ipv6>();
            Ptr<Ipv6RoutingProtocol> routing = ipv6->GetRoutingProtocol();
            NS_TEST_ASSERT_MSG_NE(routing, nullptr, "Routing protocol not installed");
        }
    }
};

class LinkFailureRecoveryTest : public TestCase
{
public:
    LinkFailureRecoveryTest() : TestCase("Test Link Failure and Recovery") {}
    virtual void DoRun()
    {
        NodeContainer nodes;
        nodes.Create(3);
        
        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
        csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

        NetDeviceContainer devices = csma.Install(nodes);

        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv6AddressHelper ipv6;
        ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
        Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);

        Simulator::Schedule(Seconds(5.0), &Node::Dispose, nodes.Get(1));
        Simulator::Run();
        Simulator::Destroy();
    }
};

static RipNgTest ripNgTest;
static RipNgRoutingTest ripNgRoutingTest;
static LinkFailureRecoveryTest linkFailureRecoveryTest;
