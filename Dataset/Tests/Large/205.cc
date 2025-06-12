#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VehicularNetworkSimulationTests");

// Test 1: Node creation
void TestNodeCreation ()
{
  NodeContainer vehicles;
  vehicles.Create (4);
  NS_TEST_ASSERT_MSG_EQ (vehicles.GetN (), 4, "Expected 4 vehicle nodes to be created");
}

// Test 2: WiFi setup (Ad-hoc communication)
void TestWifiSetup ()
{
  NodeContainer vehicles;
  vehicles.Create (4);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211g);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, vehicles);
  NS_TEST_ASSERT_MSG_EQ (devices.GetN (), 4, "Expected 4 WiFi devices to be installed");
}

// Test 3: Mobility model setup (constant velocity)
void TestMobilityModel ()
{
  NodeContainer vehicles;
  vehicles.Create (4);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (50.0),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (4),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (vehicles);

  for (uint32_t i = 0; i < vehicles.GetN (); ++i)
    {
      Ptr<ConstantVelocityMobilityModel> mobilityModel = vehicles.Get (i)->GetObject<ConstantVelocityMobilityModel> ();
      NS_TEST_ASSERT_MSG_NE (mobilityModel, nullptr, "Mobility model not installed correctly");
      NS_TEST_ASSERT_MSG_EQ (mobilityModel->GetVelocity ().x, 20.0, "Expected vehicle velocity to be 20 m/s");
    }
}

// Test 4: UDP communication setup
void TestUdpCommunication ()
{
  NodeContainer vehicles;
  vehicles.Create (4);

  uint16_t port = 9;
  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApp = echoServer.Install (vehicles.Get (0));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (Ipv4Address ("10.1.1.1"), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 1; i < vehicles.GetN (); ++i)
    {
      clientApps.Add (echoClient.Install (vehicles.Get (i)));
    }
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Run ();
  NS_TEST_ASSERT_MSG_EQ (serverApp.Get (0)->GetStartTime ().GetSeconds (), 1.0, "Server did not start at the correct time");
  NS_TEST_ASSERT_MSG_EQ (clientApps.Get (0)->GetStartTime ().GetSeconds (), 2.0, "Client did not start at the correct time");
  Simulator::Destroy ();
}

// Test 5: FlowMonitor validation
void TestFlowMonitor ()
{
  NodeContainer vehicles;
  vehicles.Create (4);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211g);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, vehicles);

  InternetStackHelper stack;
  stack.Install (vehicles);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer vehicleInterfaces = ipv4.Assign (devices);

  FlowMonitorHelper flowMonitorHelper;
  Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll ();

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  flowMonitor->CheckForLostPackets ();
  FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats ();
  NS_TEST_ASSERT_MSG_NE (stats.size (), 0, "FlowMonitor should capture some traffic");

  Simulator::Destroy ();
}

// Test 6: NetAnim positioning
void TestNetAnimPositioning ()
{
  NodeContainer vehicles;
  vehicles.Create (4);

  AnimationInterface anim ("vehicular-network-animation.xml");
  anim.SetConstantPosition (vehicles.Get (0), 0.0, 0.0);
  anim.SetConstantPosition (vehicles.Get (1), 50.0, 0.0);
  anim.SetConstantPosition (vehicles.Get (2), 100.0, 0.0);
  anim.SetConstantPosition (vehicles.Get (3), 150.0, 0.0);

  for (uint32_t i = 0; i < vehicles.GetN (); ++i)
    {
      Vector position = anim.GetPosition (vehicles.Get (i));
      NS_TEST_ASSERT_MSG_NE (position.x, 0.0, "Position X not set correctly for vehicle " + std::to_string(i));
    }
}

// Main test execution function
int main (int argc, char *argv[])
{
  TestNodeCreation ();
  TestWifiSetup ();
  TestMobilityModel ();
  TestUdpCommunication ();
  TestFlowMonitor ();
  TestNetAnimPositioning ();

  NS_LOG_UNCOND ("All tests passed successfully.");
  return 0;
}

