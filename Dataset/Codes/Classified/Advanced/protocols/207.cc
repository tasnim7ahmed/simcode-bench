#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/aodv-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpWifiExample");

int main (int argc, char *argv[])
{
  // Set the simulation time
  double simTime = 10.0;

  // Create two nodes
  NodeContainer wifiNodes;
  wifiNodes.Create (2);

  // Setup Wi-Fi physical and MAC layers
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  WifiMacHelper mac;
  mac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (phy, mac, wifiNodes);

  // Set mobility models for the nodes
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

  // Install Internet stack with AODV routing protocol
  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingHelper (aodv);
  internet.Install (wifiNodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Setup TCP server on Node 1
  uint16_t port = 9;
  Address serverAddress (InetSocketAddress (interfaces.GetAddress (1), port));
  PacketSinkHelper sink ("ns3::TcpSocketFactory", serverAddress);
  ApplicationContainer serverApp = sink.Install (wifiNodes.Get (1));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (simTime));

  // Setup TCP client on Node 0
  OnOffHelper client ("ns3::TcpSocketFactory", serverAddress);
  client.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  client.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  client.SetAttribute ("DataRate", DataRateValue (DataRate ("1Mbps")));
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApp = client.Install (wifiNodes.Get (0));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (simTime));

  // Enable NetAnim
  AnimationInterface anim ("tcp_wifi_example.xml");

  // Set up FlowMonitor to calculate throughput
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Run the simulation
  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  // Calculate throughput
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      if (t.destinationPort == port)
        {
          std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
          std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
          std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / simTime / 1000000 << " Mbps\n";
        }
    }

  // Destroy the simulation
  Simulator::Destroy ();
  return 0;
}

