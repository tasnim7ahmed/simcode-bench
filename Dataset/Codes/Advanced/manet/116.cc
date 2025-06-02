#include "ns3/core-module.h"
#include "ns3/propagation-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/wifi-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocNetworkExample");


int main (int argc, char *argv[])
{
  LogComponentEnable ("AdhocNetworkExample", LOG_LEVEL_INFO);

  // Create two wireless nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Configure wireless parameters
  WifiHelper wifi;

  WifiMacHelper mac;
  mac.SetType ("ns3::AdhocWifiMac");
  

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default(); //define channel default configuration
  YansWifiPhyHelper phy; //configure wifiPHY
  phy.SetChannel(channel.Create());

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes); //create wifiNetDevice
  

  // Set up mobility model
  MobilityHelper mobility; //to set position of nodes
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0), //starting node x axis
                                 "MinY", DoubleValue (0.0), //starting node y axis
                                 "DeltaX", DoubleValue (5.0), //gap before next node in row
                                 "DeltaY", DoubleValue (5.0), //gap before next node in column
                                 "GridWidth", UintegerValue (2), // max number of nodes based on the layout type
                                 "LayoutType", StringValue ("RowFirst")); //nodes will first fill up the rows then columns
                                 
                                
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds", RectangleValue(Rectangle(-25, 25, -25, 25))); //nodes over randomly in 2D
                                                                                          //nodes movement bound in rectangle   (Xmin,Xmax,Ymin,Ymax)        
  mobility.Install (nodes); //install the nodes
  
  // Set up network stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IPv4 addresses
  Ipv4AddressHelper address;
  address.SetBase ("192.9.39.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  UdpEchoServerHelper server10(9);
  ApplicationContainer serverApps10 = server10.Install (nodes.Get (1));
  serverApps10.Start (Seconds (1.0));
  serverApps10.Stop (Seconds (100.0));

  // Create a UDP echo client application on node 0
  UdpEchoClientHelper client10 (interfaces.GetAddress (1), 9);
  client10.SetAttribute ("MaxPackets", UintegerValue (10));
  client10.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
  client10.SetAttribute ("PacketSize", UintegerValue (1024));
  
  ApplicationContainer clientApps10 = client10.Install (nodes.Get (0));
  clientApps10.Start (Seconds (2.0));
  clientApps10.Stop (Seconds (100.0));

  UdpEchoServerHelper server01(10);
  ApplicationContainer serverApps01 = server01.Install (nodes.Get (0));
  serverApps10.Start (Seconds (1.0));
  serverApps10.Stop (Seconds (100.0));

  UdpEchoClientHelper client01 (interfaces.GetAddress (0), 10);
  client01.SetAttribute ("MaxPackets", UintegerValue (10));
  client01.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
  client01.SetAttribute ("PacketSize", UintegerValue (1024));
  
  ApplicationContainer clientApps01 = client01.Install (nodes.Get (1));
  clientApps01.Start (Seconds (2.0));
  clientApps01.Stop (Seconds (100.0));
  
  // Define a flow monitor object
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
  Simulator::Stop(Seconds(100.0));
  
  
  // Animation
  AnimationInterface anim("239_task1.xml");
  

  // Run simulation
  Simulator::Run ();
  
 
  
 Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
 std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
{
 // Calculate packet delivery ratio and end to end delay and throughput
  uint32_t packetsDelivered = 0;
  uint32_t totalPackets = 0;
  double totalDelay = 0;
 Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
std::cout << "FlowId: " << i->first << "\n";
std::cout << "Sender: "<< t.sourceAddress << "\n"; 
std::cout << "Receiver: "<< t.destinationAddress << "\n"; 
std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";

std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
if(i->second.rxPackets > 0)
  	{
  		packetsDelivered += i->second.rxPackets;
  		totalPackets += i->second.txPackets;
  		totalDelay += (i->second.delaySum.ToDouble(ns3::Time::S) / i->second.rxPackets);
  	}
       
  double packetDeliveryRatio = (double) packetsDelivered / totalPackets;
  double averageEndToEndDelay = totalDelay / packetsDelivered;
  double throughput = (double) packetsDelivered * 1024 * 8 / (100.0 - 0.0) / 1000000;
  
  // Printing the results
  std::cout << "Packets Delivered Ratio: " << packetDeliveryRatio << std::endl;
  std::cout << "Average End-to-End Delay: " << averageEndToEndDelay << std::endl;
  std::cout << "Throughput: " << throughput << std::endl;
   }
  
 
  
  Simulator::Destroy ();

  return 0;
}
