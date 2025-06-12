#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/test.h"

using namespace ns3;

// Test fixture class
class WifiSimpleAdhocGridTest : public TestCase
{
public:
    WifiSimpleAdhocGridTest() : TestCase("Wifi Simple Adhoc Grid Unit Test") {}
    virtual void DoRun();
};

// Packet reception callback
bool packetReceived = false;
void ReceiveTestPacket(Ptr<Socket> socket)
{
    while (socket->Recv())
    {
        packetReceived = true;
    }
}

// **Test 1: Packet Transmission**
void TestPacketTransmission()
{
    Ptr<Node> sender = CreateObject<Node>();
    Ptr<Socket> socket = Socket::CreateSocket(sender, UdpSocketFactory::GetTypeId());

    Simulator::Schedule(Seconds(1.0), &GenerateTraffic, socket, 1000, 1, Seconds(1.0));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_ASSERT_MSG_EQ(packetReceived, true, "Packet transmission failed!");
}

// **Test 2: Packet Reception**
void TestPacketReception()
{
    Ptr<Node> receiver = CreateObject<Node>();
    Ptr<Socket> recvSocket = Socket::CreateSocket(receiver, UdpSocketFactory::GetTypeId());
    recvSocket->SetRecvCallback(MakeCallback(&ReceiveTestPacket));

    Simulator::Schedule(Seconds(1.0), &ReceiveTestPacket, recvSocket);
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_ASSERT_MSG_EQ(packetReceived, true, "Packet reception failed!");
}

// **Test 3: Mobility Model**
void TestMobilityModel()
{
    NodeContainer nodes;
    nodes.Create(2);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(100.0),
                                  "DeltaY", DoubleValue(100.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    Ptr<MobilityModel> mobility1 = nodes.Get(0)->GetObject<MobilityModel>();
    Ptr<MobilityModel> mobility2 = nodes.Get(1)->GetObject<MobilityModel>();

    Vector pos1 = mobility1->GetPosition();
    Vector pos2 = mobility2->GetPosition();

    NS_TEST_ASSERT_MSG_EQ(pos1.x, 0.0, "Node 1 position is incorrect!");
    NS_TEST_ASSERT_MSG_EQ(pos2.x, 100.0, "Node 2 position is incorrect!");
}

// **Main Test Execution**
void WifiSimpleAdhocGridTest::DoRun()
{
    TestPacketTransmission();
    TestPacketReception();
    TestMobilityModel();
}

// **Register Test Suite**
static class WifiSimpleAdhocGridTestSuite : public TestSuite
{
public:
    WifiSimpleAdhocGridTestSuite()
        : TestSuite("wifi-simple-adhoc-grid", UNIT)
    {
        AddTestCase(new WifiSimpleAdhocGridTest, TestCase::QUICK);
    }
} g_wifiSimpleAdhocGridTestSuite;
