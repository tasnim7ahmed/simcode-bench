#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/packet.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Ns3UnitTest");

void TestNodeCreation() {
    NodeContainer nodes;
    nodes.Create(2);
    NS_ASSERT_MSG(nodes.GetN() == 2, "Node creation failed!");
    NS_LOG_INFO("TestNodeCreation passed");
}

void TestMobilitySetup() {
    NodeContainer nodes;
    nodes.Create(1);
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);
    
    Ptr<MobilityModel> mob = nodes.Get(0)->GetObject<MobilityModel>();
    NS_ASSERT_MSG(mob, "Mobility model setup failed!");
    NS_LOG_INFO("TestMobilitySetup passed");
}

void TestWifiConfiguration() {
    NodeContainer nodes;
    nodes.Create(2);
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211a);
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());
    WifiMacHelper mac;
    Ssid ssid = Ssid("test-ssid");
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);
    
    NS_ASSERT_MSG(devices.GetN() == 2, "WiFi configuration failed!");
    NS_LOG_INFO("TestWifiConfiguration passed");
}

void RunTests() {
    TestNodeCreation();
    TestMobilitySetup();
    TestWifiConfiguration();
    NS_LOG_INFO("All tests passed!");
}

int main(int argc, char *argv[]) {
    LogComponentEnable("Ns3UnitTest", LOG_LEVEL_INFO);
    RunTests();
    return 0;
}
