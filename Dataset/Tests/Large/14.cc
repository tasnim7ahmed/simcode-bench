#include "ns3/test.h"
#include "ns3/node-container.h"
#include "ns3/wifi-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/qos-wifi-mac-helper.h"
#include "ns3/ssid.h"

using namespace ns3;

class Ns3UnitTest : public TestCase {
public:
    Ns3UnitTest() : TestCase("NS-3 Unit Test") {}
    virtual void DoRun() {
        // Test node creation
        NodeContainer nodes;
        nodes.Create(2);
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Node creation failed");
        
        // Test WiFi installation
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());
        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211a);
        QosWifiMacHelper mac = QosWifiMacHelper::Default();
        Ssid ssid = Ssid("test-ssid");
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer devices = wifi.Install(phy, mac, nodes);
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "WiFi installation failed");
        
        // Test mobility setup
        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator");
        mobility.Install(nodes);
        NS_TEST_ASSERT_MSG_EQ(nodes.Get(0)->GetObject<MobilityModel>() != nullptr, true, "Mobility setup failed");
        
        // Test internet stack installation
        InternetStackHelper stack;
        stack.Install(nodes);
        NS_TEST_ASSERT_MSG_EQ(nodes.Get(0)->GetObject<Ipv4>() != nullptr, true, "Internet stack installation failed");
        
        // Test IP address assignment
        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);
        NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(0), Ipv4Address("0.0.0.0"), "IP address assignment failed");
        
        // Test application setup
        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));
        NS_TEST_ASSERT_MSG_EQ(serverApps.GetN(), 1, "Application setup failed");
    }
};

static Ns3UnitTest testInstance;
