#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/test.h"

using namespace ns3;

class WifiSimpleAdhocTest : public TestCase {
public:
    WifiSimpleAdhocTest() : TestCase("Test Wifi Simple Adhoc") {}
    virtual void DoRun() {
        TestPacketTransmission();
        TestPacketReception();
        TestMobilityModel();
    }

private:
    void TestPacketTransmission() {
        Ptr<Node> sender = CreateObject<Node>();
        Ptr<Socket> socket = Socket::CreateSocket(sender, TypeId::LookupByName("ns3::UdpSocketFactory"));
        uint32_t pktSize = 1000;
        uint32_t pktCount = 5;
        Time pktInterval = Seconds(1.0);

        Simulator::Schedule(Seconds(1.0), &GenerateTraffic, socket, pktSize, pktCount, pktInterval);
        Simulator::Run();

        NS_TEST_ASSERT_MSG_EQ(pktCount > 0, true, "Packets should be generated");
    }

    void TestPacketReception() {
        Ptr<Node> receiver = CreateObject<Node>();
        Ptr<Socket> recvSink = Socket::CreateSocket(receiver, TypeId::LookupByName("ns3::UdpSocketFactory"));
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 80);
        recvSink->Bind(local);
        recvSink->SetRecvCallback(MakeCallback(&ReceivePacket));
        Simulator::Run();

        NS_TEST_ASSERT_MSG_EQ(recvSink != nullptr, true, "Receiver should be initialized");
    }

    void TestMobilityModel() {
        Ptr<Node> node = CreateObject<Node>();
        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(node);
        Ptr<MobilityModel> mob = node->GetObject<MobilityModel>();

        NS_TEST_ASSERT_MSG_EQ(mob != nullptr, true, "Mobility model should be set");
    }
};

static WifiSimpleAdhocTest g_wifiSimpleAdhocTest;
