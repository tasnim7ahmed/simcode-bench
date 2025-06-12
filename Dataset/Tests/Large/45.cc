#include "ns3/test.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/wifi-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/internet-stack-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiSimpleInterferenceTest");

class WifiSimpleInterferenceTest : public TestCase {
public:
    WifiSimpleInterferenceTest() : TestCase("Test WiFi Simple Interference Simulation") {}
    virtual void DoRun();
};

void WifiSimpleInterferenceTest::DoRun() {
    NodeContainer c;
    c.Create(3);

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiHelper wifi;
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, c);
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(c);

    InternetStackHelper internet;
    internet.Install(c);
    
    Ptr<Socket> recvSink = Socket::CreateSocket(c.Get(0), TypeId::LookupByName("ns3::UdpSocketFactory"));
    InetSocketAddress local = InetSocketAddress(Ipv4Address("10.1.1.1"), 80);
    recvSink->Bind(local);

    Ptr<Socket> source = Socket::CreateSocket(c.Get(1), TypeId::LookupByName("ns3::UdpSocketFactory"));
    source->Connect(InetSocketAddress(Ipv4Address("10.1.1.255"), 80));
    
    source->Send(Create<Packet>(1000));
    Simulator::Run();
    Simulator::Destroy();

    NS_TEST_ASSERT_MSG_EQ(recvSink->GetRxAvailable(), 1, "Packet was not received correctly");
}

class WifiInterferenceTestSuite : public TestSuite {
public:
    WifiInterferenceTestSuite() : TestSuite("wifi-interference", UNIT) {
        AddTestCase(new WifiSimpleInterferenceTest, TestCase::QUICK);
    }
};

static WifiInterferenceTestSuite wifiInterferenceTestSuite;
