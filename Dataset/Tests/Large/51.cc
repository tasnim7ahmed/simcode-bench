#include "ns3/test.h"
#include "ns3/simulator.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/tcp-socket.h"
#include "ns3/packet-sink.h"
#include "ns3/on-off-helper.h"

using namespace ns3;

// Test Case: Verify TCP Variant is Set Properly
class TcpVariantTest : public TestCase {
public:
    TcpVariantTest() : TestCase("TCP Variant Test") {}

    virtual void DoRun() {
        std::string tcpVariant = "ns3::TcpNewReno";
        TypeId tcpTid;
        NS_TEST_ASSERT_MSG_UNLESS(TypeId::LookupByNameFailSafe(tcpVariant, &tcpTid),
                                  "TCP Variant not found");
    }
};

// Test Case: Verify Node Creation and Configuration
class NodeSetupTest : public TestCase {
public:
    NodeSetupTest() : TestCase("Node Setup Test") {}

    virtual void DoRun() {
        NodeContainer nodes;
        nodes.Create(2);
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Node count mismatch");
    }
};

// Test Case: Check if Mobility Model is Assigned
class MobilityModelTest : public TestCase {
public:
    MobilityModelTest() : TestCase("Mobility Model Test") {}

    virtual void DoRun() {
        NodeContainer nodes;
        nodes.Create(2);
        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(nodes);
        NS_TEST_ASSERT_MSG_NE(nodes.Get(0)->GetObject<MobilityModel>(), nullptr,
                              "Mobility Model not installed");
    }
};

// Test Case: Validate Packet Transmission
class PacketTransmissionTest : public TestCase {
public:
    PacketTransmissionTest() : TestCase("Packet Transmission Test") {}

    virtual void DoRun() {
        NodeContainer nodes;
        nodes.Create(2);
        InternetStackHelper stack;
        stack.Install(nodes);

        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), 9));
        ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(0));
        sinkApp.Start(Seconds(0.0));

        OnOffHelper server("ns3::TcpSocketFactory",
                           InetSocketAddress(Ipv4Address("10.0.0.1"), 9));
        server.SetAttribute("DataRate", DataRateValue(DataRate("100Mb/s")));
        ApplicationContainer serverApp = server.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));

        Simulator::Stop(Seconds(2.0));
        Simulator::Run();
        Simulator::Destroy();

        Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp.Get(0));
        NS_TEST_ASSERT_MSG_GT(sink->GetTotalRx(), 0, "No packets received");
    }
};

// Define the test suite
class WifiTcpTestSuite : public TestSuite {
public:
    WifiTcpTestSuite() : TestSuite("wifi-tcp-tests", UNIT) {
        AddTestCase(new TcpVariantTest, TestCase::QUICK);
        AddTestCase(new NodeSetupTest, TestCase::QUICK);
        AddTestCase(new MobilityModelTest, TestCase::QUICK);
        AddTestCase(new PacketTransmissionTest, TestCase::EXTENSIVE);
    }
};

// Register the test suite
static WifiTcpTestSuite wifiTcpTestSuite;
