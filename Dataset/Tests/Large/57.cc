#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

class StarTopologyTest : public TestCase {
public:
    StarTopologyTest() : TestCase("Star Topology Test") {}
    virtual void DoRun() {
        uint32_t nSpokes = 8;
        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
        
        PointToPointStarHelper star(nSpokes, pointToPoint);
        
        NS_TEST_ASSERT_MSG_EQ(star.SpokeCount(), nSpokes, "Incorrect number of spokes");
        NS_TEST_ASSERT_MSG_EQ(star.GetHub()->GetNDevices(), nSpokes, "Hub device count mismatch");
    }
};

class InternetStackInstallationTest : public TestCase {
public:
    InternetStackInstallationTest() : TestCase("Internet Stack Installation Test") {}
    virtual void DoRun() {
        uint32_t nSpokes = 8;
        PointToPointHelper pointToPoint;
        PointToPointStarHelper star(nSpokes, pointToPoint);
        
        InternetStackHelper internet;
        star.InstallStack(internet);
        
        for (uint32_t i = 0; i < star.SpokeCount(); ++i) {
            NS_TEST_ASSERT_MSG_NE(star.GetSpokeNode(i)->GetObject<Ipv4>(), nullptr, "Internet stack not installed");
        }
    }
};

class IPAddressAssignmentTest : public TestCase {
public:
    IPAddressAssignmentTest() : TestCase("IP Address Assignment Test") {}
    virtual void DoRun() {
        uint32_t nSpokes = 8;
        PointToPointHelper pointToPoint;
        PointToPointStarHelper star(nSpokes, pointToPoint);
        
        InternetStackHelper internet;
        star.InstallStack(internet);
        
        star.AssignIpv4Addresses(Ipv4AddressHelper("10.1.1.0", "255.255.255.0"));
        
        for (uint32_t i = 0; i < star.SpokeCount(); ++i) {
            Ipv4InterfaceAddress addr = star.GetSpokeNode(i)->GetObject<Ipv4>()->GetAddress(1, 0);
            NS_TEST_ASSERT_MSG_NE(addr.GetLocal(), Ipv4Address("0.0.0.0"), "IP address not assigned");
        }
    }
};

class PacketTransmissionTest : public TestCase {
public:
    PacketTransmissionTest() : TestCase("Packet Transmission Test") {}
    virtual void DoRun() {
        uint32_t nSpokes = 8;
        PointToPointHelper pointToPoint;
        PointToPointStarHelper star(nSpokes, pointToPoint);
        
        InternetStackHelper internet;
        star.InstallStack(internet);
        star.AssignIpv4Addresses(Ipv4AddressHelper("10.1.1.0", "255.255.255.0"));
        
        uint16_t port = 50000;
        Address hubLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", hubLocalAddress);
        ApplicationContainer hubApp = packetSinkHelper.Install(star.GetHub());
        hubApp.Start(Seconds(1.0));
        hubApp.Stop(Seconds(10.0));
        
        OnOffHelper onOffHelper("ns3::TcpSocketFactory", Address());
        onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        
        ApplicationContainer spokeApps;
        for (uint32_t i = 0; i < star.SpokeCount(); ++i) {
            AddressValue remoteAddress(InetSocketAddress(star.GetHubIpv4Address(i), port));
            onOffHelper.SetAttribute("Remote", remoteAddress);
            spokeApps.Add(onOffHelper.Install(star.GetSpokeNode(i)));
        }
        spokeApps.Start(Seconds(1.0));
        spokeApps.Stop(Seconds(10.0));
        
        Simulator::Run();
        Simulator::Destroy();
        
        Ptr<PacketSink> sink = hubApp.Get(0)->GetObject<PacketSink>();
        NS_TEST_ASSERT_MSG_GT(sink->GetTotalRx(), 0, "No packets received at the hub");
    }
};

static StarTopologyTest starTopologyTest;
static InternetStackInstallationTest internetStackTest;
static IPAddressAssignmentTest ipAssignmentTest;
static PacketTransmissionTest packetTransmissionTest;
