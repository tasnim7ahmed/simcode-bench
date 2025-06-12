#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/test.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiHandoverTest");

class WifiHandoverTestSuite : public TestSuite
{
public:
  WifiHandoverTestSuite () : TestSuite ("wifi-handover-test", UNIT)
  {
    AddTestCase (new WifiHandoverTestCase (), TestCase::QUICK);
  }
};

class WifiHandoverTestCase : public TestCase
{
public:
  WifiHandoverTestCase () : TestCase ("Test Wifi Handover Example") {}
  
  virtual void DoRun (void)
  {
    // Test 1: Check AP and Station node creation
    NodeContainer wifiApNodes;
    wifiApNodes.Create (2);
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create (6);
    
    NS_TEST_ASSERT_MSG_EQ (wifiApNodes.GetN (), 2, "Expected 2 AP nodes");
    NS_TEST_ASSERT_MSG_EQ (wifiStaNodes.GetN (), 6, "Expected 6 Station nodes");
    
    // Set up Wi-Fi PHY and channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetChannel (channel.Create ());

    // Set up Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
    WifiMacHelper mac;
    Ssid ssid = Ssid ("HandoverNetwork");

    // Access Point setup
    mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
    NetDeviceContainer apDevices = wifi.Install (phy, mac, wifiApNodes);
    NS_TEST_ASSERT_MSG_EQ (apDevices.GetN (), 2, "Expected 2 AP devices");

    // Station setup
    mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid));
    NetDeviceContainer staDevices = wifi.Install (phy, mac, wifiStaNodes);
    NS_TEST_ASSERT_MSG_EQ (staDevices.GetN (), 6, "Expected 6 STA devices");

    // Test 2: Mobility model for static APs
    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiApNodes);

    for (uint32_t i = 0; i < wifiApNodes.GetN (); ++i)
      {
        Ptr<MobilityModel> mobilityModel = wifiApNodes.Get (i)->GetObject<MobilityModel> ();
        NS_TEST_ASSERT_MSG_NE (mobilityModel, nullptr, "AP node mobility model not installed correctly on node " + std::to_string (i));
      }

    // Test 3: Mobility model for mobile STAs
    MobilityHelper mobilitySta;
    mobilitySta.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                  "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
    mobilitySta.Install (wifiStaNodes);

    for (uint32_t i = 0; i < wifiStaNodes.GetN (); ++i)
      {
        Ptr<MobilityModel> mobilityModel = wifiStaNodes.Get (i)->GetObject<MobilityModel> ();
        NS_TEST_ASSERT_MSG_NE (mobilityModel, nullptr, "STA node mobility model not installed correctly on node " + std::to_string (i));
      }

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install (wifiApNodes);
    stack.Install (wifiStaNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = address.Assign (apDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);

    // Test 4: Verify UDP echo server setup
    UdpEchoServerHelper echoServer (9);
    ApplicationContainer serverApps = echoServer.Install (wifiApNodes.Get (0));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (20.0));

    NS_TEST_ASSERT_MSG_EQ (serverApps.GetN (), 1, "Expected 1 server application on AP node 0");

    // Test 5: Verify UDP echo client setup
    UdpEchoClientHelper echoClient (apInterfaces.GetAddress (0), 9);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

    ApplicationContainer clientApps = echoClient.Install (wifiStaNodes);
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (20.0));

    NS_TEST_ASSERT_MSG_EQ (clientApps.GetN (), 6, "Expected 6 client applications on the STA nodes");

    // Test 6: Enable FlowMonitor and validate
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
    Simulator::Stop (Seconds (20.0));
    Simulator::Run ();

    // Verify flow monitor statistics
    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

    bool throughputTestPassed = false;
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
      {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
        if (t.sourceAddress == staInterfaces.GetAddress (0) && t.destinationAddress == apInterfaces.GetAddress (0))
          {
            double throughput = i->second.rxBytes * 8.0 / (20.0 - 1.0) / 1000000; // 20s simulation, 1s delay
            NS_TEST_ASSERT_MSG_GT (throughput, 0.0, "Expected positive throughput from the flow");
            throughputTestPassed = true;
          }
      }
    
    NS_TEST_ASSERT_MSG_EQ (throughputTestPassed, true, "No valid throughput was measured");

    Simulator::Destroy ();
  }
};

// Declare the test suite
static WifiHandoverTestSuite wifiHandoverTestSuite;

int main (int argc, char *argv[])
{
  NS_LOG_UNCOND ("Running WiFi Handover Test...");
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}

