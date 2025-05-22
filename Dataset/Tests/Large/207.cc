#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/aodv-module.h"
#include "ns3/test.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpWifiExampleTest");

// Test 1: Node creation
void TestNodeCreation ()
{
  NodeContainer wifiNodes;
  wifiNodes.Create (2);
  
  NS_TEST_ASSERT_MSG_EQ (wifiNodes.GetN (), 2, "Expected 2 nodes to be created");
}

// Test 2: Wi-Fi setup
void TestWifiSetup ()
{
  NodeContainer wifiNodes;
  wifiNodes.Create (2);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  WifiMacHelper mac;
  mac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (phy, mac, wifiNodes);
  
  NS_TEST_ASSERT_MSG_EQ (devices.GetN (), 2, "Expected 2 Wi-Fi devices to be installed (1 for each node)");
}

// Test 3: Mobility model setup
void TestMobilityModel ()
{
  NodeContainer wifiNodes;
  wifiNodes.Create (2);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (2),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiNodes);

  for (uint32_t i = 0; i < wifiNodes.GetN (); ++i)
    {
      Ptr<MobilityModel> mobilityModel = wifiNodes.Get (i)->GetObject<MobilityModel> ();
      NS_TEST_ASSERT_MSG_NE (mobilityModel, nullptr, "Mobility model not installed correctly on node " + std::to_string(i));
    }
}

// Test 4: TCP server-client setup
void TestTcpCommunication ()
{
  NodeContainer wifiNodes;
  wifiNodes.Create (2);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  WifiMacHelper mac;
  mac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (phy, mac, wifiNodes);

  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingHelper (aodv);
  internet.Install (wifiNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  uint16_t port = 9;
  Address serverAddress (InetSocketAddress (interfaces.GetAddress (1), port));

  PacketSinkHelper sink ("ns3::TcpSocketFactory", serverAddress);
  ApplicationContainer serverApp = sink.Install (wifiNodes.Get (1));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (10.0));

  OnOffHelper client ("ns3::TcpSocketFactory", serverAddress);
  client.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  client.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  client.SetAttribute ("DataRate", DataRateValue (DataRate ("1Mbps")));
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApp = client.Install (wifiNodes.Get (0));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (10.0));

  Simulator::Run ();
  
  // Test if server application started at the correct time
  NS_TEST_ASSERT_MSG_EQ (serverApp.Get (0)->GetStartTime ().GetSeconds (), 1.0, "Server did not start at the correct time");

  Simulator::Destroy ();
}

// Test 5: FlowMonitor validation
void TestFlowMonitor ()
{
  NodeContainer wifiNodes;
  wifiNodes.Create (2);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  WifiMacHelper mac;
  mac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (phy, mac, wifiNodes);

  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingHelper (aodv);
  internet.Install (wifiNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  uint16_t port = 9;
  Address serverAddress (InetSocketAddress (interfaces.GetAddress (1), port));

  PacketSinkHelper sink ("ns3::TcpSocketFactory", serverAddress);
  ApplicationContainer serverApp = sink.Install (wifiNodes.Get (1));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (10.0));

  OnOffHelper client ("ns3::TcpSocketFactory", serverAddress);
  client.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  client.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  client.SetAttribute ("DataRate", DataRateValue (DataRate ("1Mbps")));
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApp = client.Install (wifiNodes.Get (0));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (10.0));

  // Enable FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      if (t.destinationPort == port)
        {
          double throughput = i->second.rxBytes * 8.0 / 10.0 / 1000000; // 10 seconds simulation
          NS_TEST_ASSERT_MSG_GT (throughput, 0.0, "Throughput is expected to be greater than 0 Mbps");
        }
    }

  Simulator::Destroy ();
}

// Test 6: NetAnim positioning
void TestNetAnimPositioning ()
{
  NodeContainer wifiNodes;
  wifiNodes.Create (2);

  AnimationInterface anim ("tcp_wifi_example.xml");

  // Test positions of both nodes in NetAnim
  anim.SetConstantPosition (wifiNodes.Get (0), 0.0, 0.0);
  anim.SetConstantPosition (wifiNodes.Get (1), 5.0, 0.0);

  for (uint32_t i = 0; i < wifiNodes.GetN (); ++i)
    {
      Vector position = anim.GetPosition (wifiNodes.Get (i));
      NS_TEST_ASSERT_MSG_EQ (position.x, i * 5.0, "Node " + std::to_string(i) + " not positioned correctly in NetAnim");
    }
}

// Main test execution function
int main (int argc, char *argv[])
{
  TestNodeCreation ();
  TestWifiSetup ();
  TestMobilityModel ();
  TestTcpCommunication ();
  TestFlowMonitor ();
  TestNetAnimPositioning ();

  NS_LOG_UNCOND ("All tests passed successfully.");
  return 0;
}

