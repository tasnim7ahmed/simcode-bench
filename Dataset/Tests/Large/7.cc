#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

class RingTopologyTest : public TestCase {
public:
    RingTopologyTest() : TestCase("Test Ring Topology") {}

    virtual void DoRun() {
        NodeContainer ring;
        ring.Create(4);
        NS_TEST_ASSERT_MSG_EQ(ring.GetN(), 4, "Should create 4 nodes");
    }
};

class PointToPointLinkTest : public TestCase {
public:
    PointToPointLinkTest() : TestCase("Test Point-to-Point Links") {}

    virtual void DoRun() {
        NodeContainer nodes;
        nodes.Create(2);
        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
        NetDeviceContainer devices = pointToPoint.Install(nodes);
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Each link should have 2 devices");
    }
};

class IPAddressAssignmentTest : public TestCase {
public:
    IPAddressAssignmentTest() : TestCase("Test IP Address Assignment") {}

    virtual void DoRun() {
        NodeContainer nodes;
        nodes.Create(2);
        InternetStackHelper stack;
        stack.Install(nodes);
        
        PointToPointHelper pointToPoint;
        NetDeviceContainer devices = pointToPoint.Install(nodes);
        
        Ipv4AddressHelper address;
        address.SetBase("192.9.39.0", "255.255.255.252");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);
        
        NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(0), Ipv4Address("0.0.0.0"), "First node should have an assigned IP");
        NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(1), Ipv4Address("0.0.0.0"), "Second node should have an assigned IP");
    }
};

class UdpEchoTest : public TestCase {
public:
    UdpEchoTest() : TestCase("Test UDP Echo Server & Client") {}

    virtual void DoRun() {
        NodeContainer nodes;
        nodes.Create(2);
        InternetStackHelper stack;
        stack.Install(nodes);

        PointToPointHelper pointToPoint;
        NetDeviceContainer devices = pointToPoint.Install(nodes);
        
        Ipv4AddressHelper address;
        address.SetBase("192.9.39.0", "255.255.255.252");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(1));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        Simulator::Run();
        Simulator::Destroy();
    }
};

static RingTopologyTest ringTopologyTest;
static PointToPointLinkTest pointToPointLinkTest;
static IPAddressAssignmentTest ipAddressAssignmentTest;
static UdpEchoTest udpEchoTest;
