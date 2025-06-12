#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class TbfQueueDiscTest : public TestCase {
public:
    TbfQueueDiscTest() : TestCase("Test TBF Queue Discipline") {}

    virtual void DoRun() {
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("2Mb/s"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("0ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);
        InternetStackHelper stack;
        stack.Install(nodes);

        TrafficControlHelper tch;
        tch.SetRootQueueDisc("ns3::TbfQueueDisc");
        QueueDiscContainer qdiscs = tch.Install(devices);

        Ptr<QueueDisc> q = qdiscs.Get(1);
        NS_TEST_ASSERT_MSG_NE(q, nullptr, "QueueDisc not installed correctly");
    }
};

class PacketTransmissionTest : public TestCase {
public:
    PacketTransmissionTest() : TestCase("Test Packet Transmission") {}

    virtual void DoRun() {
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("2Mb/s"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("0ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);
        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        uint16_t port = 7;
        Address localAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", localAddress);
        ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(0));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(10.1));

        OnOffHelper onoff("ns3::TcpSocketFactory", Ipv4Address::GetAny());
        onoff.SetAttribute("DataRate", StringValue("1.1Mb/s"));
        onoff.SetAttribute("PacketSize", UintegerValue(1448));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.2]"));
        onoff.SetAttribute("Remote", AddressValue(InetSocketAddress(interfaces.GetAddress(0), port)));

        ApplicationContainer apps = onoff.Install(nodes.Get(1));
        apps.Start(Seconds(1.0));
        apps.Stop(Seconds(10.1));

        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_ASSERT_MSG_EQ(sinkApp.Get(0)->GetObject<PacketSink>()->GetTotalRx() > 0, true, "No packets received!");
    }
};

static class TbfTestSuite : public TestSuite {
public:
    TbfTestSuite() : TestSuite("tbf-test", UNIT) {
        AddTestCase(new TbfQueueDiscTest, TestCase::QUICK);
        AddTestCase(new PacketTransmissionTest, TestCase::QUICK);
    }
} tbfTestSuiteInstance;
