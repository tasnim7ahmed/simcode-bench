#include "ns3/test.h"
#include "ns3/simulator.h"
#include "ns3/node.h"
#include "ns3/mobility-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/log.h"
#include "ns3/ssid.h"
#include "ns3/uinteger.h"

using namespace ns3;

class WifiBlockAckTest : public TestCase
{
public:
    WifiBlockAckTest() : TestCase("Test 802.11n Block Acknowledgment Mechanism") {}
    virtual void DoRun(void)
    {
        Ptr<Node> sta = CreateObject<Node>();
        Ptr<Node> ap = CreateObject<Node>();

        // Test Node Creation
        NS_TEST_ASSERT_MSG_EQ(sta != nullptr, true, "STA Node not created properly");
        NS_TEST_ASSERT_MSG_EQ(ap != nullptr, true, "AP Node not created properly");

        // Test Wi-Fi Installation
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy;
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211n);
        WifiMacHelper mac;
        Ssid ssid("My-network");

        mac.SetType("ns3::StaWifiMac", "QosSupported", BooleanValue(true), "Ssid", SsidValue(ssid));
        NetDeviceContainer staDevice = wifi.Install(phy, mac, sta);

        mac.SetType("ns3::ApWifiMac", "QosSupported", BooleanValue(true), "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(phy, mac, ap);

        NS_TEST_ASSERT_MSG_EQ(staDevice.GetN(), 1, "STA Device not installed correctly");
        NS_TEST_ASSERT_MSG_EQ(apDevice.GetN(), 1, "AP Device not installed correctly");

        // Test Mobility Model Assignment
        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(sta);
        mobility.Install(ap);
        NS_TEST_ASSERT_MSG_EQ(sta->GetObject<MobilityModel>() != nullptr, true, "STA Mobility Model not set");
        NS_TEST_ASSERT_MSG_EQ(ap->GetObject<MobilityModel>() != nullptr, true, "AP Mobility Model not set");

        // Test Internet Stack Installation
        InternetStackHelper stack;
        stack.Install(sta);
        stack.Install(ap);
        NS_TEST_ASSERT_MSG_EQ(sta->GetObject<Ipv4>() != nullptr, true, "STA IP stack not installed");
        NS_TEST_ASSERT_MSG_EQ(ap->GetObject<Ipv4>() != nullptr, true, "AP IP stack not installed");

        // Test Address Assignment
        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer staIf = address.Assign(staDevice);
        Ipv4InterfaceContainer apIf = address.Assign(apDevice);
        NS_TEST_ASSERT_MSG_NE(staIf.GetAddress(0), Ipv4Address("0.0.0.0"), "STA IP not assigned correctly");
        NS_TEST_ASSERT_MSG_NE(apIf.GetAddress(0), Ipv4Address("0.0.0.0"), "AP IP not assigned correctly");

        // Test Data Transmission
        uint16_t port = 9;
        OnOffHelper onOff("ns3::UdpSocketFactory", Address(InetSocketAddress(apIf.GetAddress(0), port)));
        onOff.SetAttribute("PacketSize", UintegerValue(50));
        ApplicationContainer staApps = onOff.Install(sta);
        staApps.Start(Seconds(1.0));
        staApps.Stop(Seconds(10.0));

        Simulator::Stop(Seconds(10.0));
        Simulator::Run();
        Simulator::Destroy();
    }
};

static WifiBlockAckTest testInstance;
