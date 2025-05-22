#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/test.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimpleSensorNetworkTest");

// Test 1: Node creation
void TestNodeCreation ()
{
  NodeContainer sensorNodes;
  sensorNodes.Create (5);
  
  NodeContainer sinkNode;
  sinkNode.Create (1);

  NS_TEST_ASSERT_MSG_EQ (sensorNodes.GetN (), 5, "Expected 5 sensor nodes to be created");
  NS_TEST_ASSERT_MSG_EQ (sinkNode.GetN (), 1, "Expected 1 sink node to be created");
}

// Test 2: WiFi setup
void TestWifiSetup ()
{
  NodeContainer sensorNodes;
  sensorNodes.Create (5);
  
  NodeContainer sinkNode;
  sinkNode.Create (1);

  NodeContainer allNodes = NodeContainer (sensorNodes, sinkNode);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211_15_4);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, allNodes);
  
  NS_TEST_ASSERT_MSG_EQ (devices.GetN (), 6, "Expected 6 WiFi devices to be installed (5 sensors + 1 sink)");
}

// Test 3: Mobility model setup
void TestMobilityModel ()
{
  NodeContainer sensorNodes;
  sensorNodes.Create (5);
  
  NodeContainer sinkNode;
  sinkNode.Create (1);

  NodeContainer allNodes = NodeContainer (sensorNodes, sinkNode);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (50.0, 50.0, 0.0));  // Sink node position

  // Sensor node positions
  positionAlloc->Add (Vector (100.0, 50.0, 0.0));
  positionAlloc->Add (Vector (50.0, 100.0, 0.0));
  positionAlloc->Add (Vector (0.0, 50.0, 0.0));
  positionAlloc->Add (Vector (50.0, 0.0, 0.0));
  positionAlloc->Add (Vector (75.0, 75.0, 0.0));

  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (allNodes);

  for (uint32_t i = 0; i < allNodes.GetN (); ++i)
    {
      Ptr<MobilityModel> mobilityModel = allNodes.Get (i)->GetObject<MobilityModel> ();
      NS_TEST_ASSERT_MSG_NE (mobilityModel, nullptr, "Mobility model not installed correctly on node " + std::to_string(i));
    }
}

// Test 4: UDP communication setup
void TestUdpCommunication ()
{
  NodeContainer sensorNodes;
  sensorNodes.Create (5);
  
  NodeContainer sinkNode;
  sinkNode.Create (1);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApp = echoServer.Install (sinkNode.Get (0));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (Ipv4Address ("10.1.1.6"), 9); // Sink node's IP address
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < sensorNodes.GetN (); ++i)
    {
      clientApps.Add (echoClient.Install (sensorNodes.Get (i)));
    }
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Run ();
  NS_TEST_ASSERT_MSG_EQ (serverApp.Get (0)->GetStartTime ().GetSeconds (), 1.0, "Server did not start at the correct time");
  Simulator::Destroy ();
}

// Test 5: FlowMonitor validation
void TestFlowMonitor ()
{
  NodeContainer sensorNodes;
  sensorNodes.Create (5);
  
  NodeContainer sinkNode;
  sinkNode.Create (1);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211_15_4);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, NodeContainer (sensorNodes, sinkNode));

  InternetStackHelper stack;
  stack.Install (NodeContainer (sensorNodes, sinkNode));

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  address.Assign (devices);

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  NS_TEST_ASSERT_MSG_NE (monitor->GetFlowStats ().size (), 0, "FlowMonitor should capture some traffic");

  Simulator::Destroy ();
}

// Test 6: NetAnim positioning
void TestNetAnimPositioning ()
{
  NodeContainer sensorNodes;
  sensorNodes.Create (5);
  
  NodeContainer sinkNode;
  sinkNode.Create (1);

  AnimationInterface anim ("sensor-network-animation.xml");
  anim.SetConstantPosition (sinkNode.Get (0), 50.0, 50.0);

  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (100.0, 50.0, 0.0));
  positionAlloc->Add (Vector (50.0, 100.0, 0.0));
  positionAlloc->Add (Vector (0.0, 50.0, 0.0));
  positionAlloc->Add (Vector (50.0, 0.0, 0.0));
  positionAlloc->Add (Vector (75.0, 75.0, 0.0));

  for (uint32_t i = 0; i < sensorNodes.GetN (); ++i)
    {
      anim.SetConstantPosition (sensorNodes.Get (i), positionAlloc->GetNext ());
    }

  for (uint32_t i = 0; i < sensorNodes.GetN (); ++i)
    {
      Vector position = anim.GetPosition (sensorNodes.Get (i));
      NS_TEST_ASSERT_MSG_NE (position.x, 0.0, "Position X not set correctly for sensor node " + std::to_string(i));
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

