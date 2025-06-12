#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/test.h"
#include "experiment.h"

using namespace ns3;

class ExperimentTestSuite : public TestSuite {
public:
    ExperimentTestSuite() : TestSuite("experiment-tests", UNIT) {
        AddTestCase(new TestNodeCreation, TestCase::QUICK);
        AddTestCase(new TestMobilitySetup, TestCase::QUICK);
        AddTestCase(new TestPacketReception, TestCase::QUICK);
        AddTestCase(new TestTrafficGeneration, TestCase::QUICK);
    }
};

class TestNodeCreation : public TestCase {
public:
    TestNodeCreation() : TestCase("Test Node Creation") {}
    void DoRun() override {
        NodeContainer nodes;
        nodes.Create(2);
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Node creation failed");
    }
};

class TestMobilitySetup : public TestCase {
public:
    TestMobilitySetup() : TestCase("Test Mobility Setup") {}
    void DoRun() override {
        NodeContainer nodes;
        nodes.Create(1);

        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(nodes);

        Ptr<MobilityModel> mobilityModel = nodes.Get(0)->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_NE(mobilityModel, nullptr, "Mobility model setup failed");
    }
};

class TestPacketReception : public TestCase {
public:
    TestPacketReception() : TestCase("Test Packet Reception") {}
    void DoRun() override {
        Experiment experiment;
        NodeContainer nodes;
        nodes.Create(2);

        Ptr<Socket> recvSocket = experiment.SetupPacketReceive(nodes.Get(0));
        NS_TEST_ASSERT_MSG_NE(recvSocket, nullptr, "Packet receive setup failed");
    }
};

class TestTrafficGeneration : public TestCase {
public:
    TestTrafficGeneration() : TestCase("Test Traffic Generation") {}
    void DoRun() override {
        Experiment experiment;
        NodeContainer nodes;
        nodes.Create(2);

        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        Ptr<Socket> source = Socket::CreateSocket(nodes.Get(1), tid);
        InetSocketAddress remote = InetSocketAddress(Ipv4Address("255.255.255.255"), 80);
        source->SetAllowBroadcast(true);
        source->Connect(remote);

        Simulator::Schedule(Seconds(1.0), &Experiment::GenerateTraffic, &experiment, source, 1024, 10, Seconds(1.0));
        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_ASSERT_MSG_GT(experiment.Run(WifiHelper(), YansWifiPhyHelper(), WifiMacHelper(), YansWifiChannelHelper()), 0, "No packets were received");
    }
};

static ExperimentTestSuite experimentTestSuite;
