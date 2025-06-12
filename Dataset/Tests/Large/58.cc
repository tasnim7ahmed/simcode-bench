#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/test.h"

using namespace ns3;

class NetworkTopologyTest : public TestCase {
public:
    NetworkTopologyTest() : TestCase("Test network topology setup") {}

    void DoRun() override {
        NodeContainer sender, receiver, routers;
        sender.Create(1);
        receiver.Create(1);
        routers.Create(2);

        // Check node count
        NS_TEST_ASSERT_MSG_EQ(sender.GetN(), 1, "Sender node count incorrect");
        NS_TEST_ASSERT_MSG_EQ(receiver.GetN(), 1, "Receiver node count incorrect");
        NS_TEST_ASSERT_MSG_EQ(routers.GetN(), 2, "Router node count incorrect");
    }
};

class InternetStackTest : public TestCase {
public:
    InternetStackTest() : TestCase("Test Internet Stack installation") {}

    void DoRun() override {
        NodeContainer nodes;
        nodes.Create(4);
        InternetStackHelper internet;
        internet.Install(nodes);

        for (uint32_t i = 0; i < nodes.GetN(); ++i) {
            Ptr<Node> node = nodes.Get(i);
            NS_TEST_ASSERT_MSG_NE(node->GetObject<Ipv4>(), nullptr, "Internet stack not installed on node " << i);
        }
    }
};

class IPAddressAssignmentTest : public TestCase {
public:
    IPAddressAssignmentTest() : TestCase("Test IP address assignment") {}

    void DoRun() override {
        NodeContainer nodes;
        nodes.Create(2);
        InternetStackHelper internet;
        internet.Install(nodes);

        PointToPointHelper pointToPoint;
        NetDeviceContainer devices = pointToPoint.Install(nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.0.0.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

        NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(0), Ipv4Address("0.0.0.0"), "Invalid IP address assigned");
    }
};

class PacketTransmissionTest : public TestCase {
public:
    PacketTransmissionTest() : TestCase("Test packet transmission") {}

    void DoRun() override {
        NodeContainer sender, receiver;
        sender.Create(1);
        receiver.Create(1);

        InternetStackHelper internet;
        internet.Install(sender);
        internet.Install(receiver);

        PointToPointHelper pointToPoint;
        NetDeviceContainer devices = pointToPoint.Install(sender.Get(0), receiver.Get(0));

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.0.0.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

        uint16_t port = 50001;
        BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
        source.SetAttribute("MaxBytes", UintegerValue(0));
        ApplicationContainer sourceApps = source.Install(sender.Get(0));
        sourceApps.Start(Seconds(0.1));

        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApps = sink.Install(receiver.Get(0));
        sinkApps.Start(Seconds(0.0));

        Simulator::Run();
        Simulator::Destroy();
    }
};

static NetworkTopologyTest networkTopologyTest;
static InternetStackTest internetStackTest;
static IPAddressAssignmentTest ipAddressAssignmentTest;
static PacketTransmissionTest packetTransmissionTest;