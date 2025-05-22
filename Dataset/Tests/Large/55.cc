#include "ns3/test.h"
#include "ns3/node-container.h"
#include "ns3/net-device-container.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/csma-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/ssid.h"

using namespace ns3;

class WifiSimulationTest : public TestCase {
public:
    WifiSimulationTest() : TestCase("Test ns-3 Wifi Simulation") {}

    virtual void DoRun() {
        TestNodeCreation();
        TestInternetStackInstallation();
        TestCsmaInstallation();
        TestWifiSetup();
    }

private:
    void TestNodeCreation() {
        NodeContainer backboneNodes;
        uint32_t nWifis = 2;
        backboneNodes.Create(nWifis);
        
        NS_TEST_ASSERT_MSG_EQ(backboneNodes.GetN(), nWifis, "Backbone nodes were not created correctly");
    }

    void TestInternetStackInstallation() {
        NodeContainer nodes;
        nodes.Create(2);
        InternetStackHelper stack;
        stack.Install(nodes);
        
        NS_TEST_ASSERT_MSG_EQ(nodes.Get(0)->GetObject<Ipv4>() != nullptr, true, "Internet stack not installed on node 0");
        NS_TEST_ASSERT_MSG_EQ(nodes.Get(1)->GetObject<Ipv4>() != nullptr, true, "Internet stack not installed on node 1");
    }

    void TestCsmaInstallation() {
        NodeContainer nodes;
        nodes.Create(2);
        CsmaHelper csma;
        NetDeviceContainer devices = csma.Install(nodes);
        
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "CSMA devices were not installed correctly");
    }

    void TestWifiSetup() {
        NodeContainer staNodes;
        staNodes.Create(2);
        
        YansWifiPhyHelper wifiPhy;
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());
        
        WifiHelper wifi;
        WifiMacHelper wifiMac;
        Ssid ssid = Ssid("test-ssid");
        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        
        NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, staNodes);
        
        NS_TEST_ASSERT_MSG_EQ(staDevices.GetN(), 2, "WiFi STA devices were not installed correctly");
    }
};

static WifiSimulationTest g_wifiSimulationTest;
