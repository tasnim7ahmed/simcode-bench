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

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimpleSensorNetwork");

int main (int argc, char *argv[])
{
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create 6 nodes (5 sensors + 1 sink)
  NodeContainer sensorNodes;
  sensorNodes.Create (5);
  
  NodeContainer sinkNode;
  sinkNode.Create (1);

  NodeContainer allNodes = NodeContainer (sensorNodes, sinkNode);

  // Set up WiFi 802.15.4
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211_15_4);
  
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  
  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");
  
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, allNodes);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (allNodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Set up mobility model for sensor nodes (fixed positions)
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();

  // Position the sink node at the center
  positionAlloc->Add (Vector (50.0, 50.0, 0.0));

  // Position the sensor nodes around the sink in a star topology
  positionAlloc->Add (Vector (100.0, 50.0, 0.0));  // Sensor 1
  positionAlloc->Add (Vector (50.0, 100.0, 0.0));  // Sensor 2
  positionAlloc->Add (Vector (0.0, 50.0, 0.0));    // Sensor 3
  positionAlloc->Add (Vector (50.0, 0.0, 0.0));    // Sensor 4
  positionAlloc->Add (Vector (75.0, 75.0, 0.0));   // Sensor 5

  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (allNodes);

  // Set up UDP communication between sensors and sink
  UdpEchoServerHelper echoServer (9);  // Port 9 for sink node
  ApplicationContainer serverApps = echoServer.Install (sinkNode.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (5), 9); // Sink address
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0))); // Periodic data
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024)); // 1 KB data

  // Install client applications on sensor nodes
  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < sensorNodes.GetN (); ++i)
    {
      clientApps.Add (echoClient.Install (sensorNodes.Get (i)));
    }
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Set up FlowMonitor for measuring performance
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Set up NetAnim for visualization
  AnimationInterface anim ("sensor-network.xml");
  anim.SetConstantPosition (sinkNode.Get (0), 50.0, 50.0);  // Sink at center
  for (uint32_t i = 0; i < sensorNodes.GetN (); ++i)
    {
      anim.SetConstantPosition (sensorNodes.Get (i), positionAlloc->GetNext ());
    }

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  // FlowMonitor analysis
  monitor->SerializeToXmlFile ("sensor-network-flowmon.xml", true, true);

  Simulator::Destroy ();
  return 0;
}

