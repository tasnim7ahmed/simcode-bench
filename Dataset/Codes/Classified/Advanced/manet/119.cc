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
  nodes.Create (6);

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
                                 "DeltaX", DoubleValue (2.0), //gap before next node in row
                                 "DeltaY", DoubleValue (2.0), //gap before next node in column
                                 "GridWidth", UintegerValue (5), // max number of nodes based on the layout type
                                 "LayoutType", StringValue ("RowFirst")); //nodes will first fill up the rows then columns
                                 
                                
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds", RectangleValue(Rectangle(-100, 100, -100,100))); //nodes over randomly in 2D
                                                                                          //nodes movement bound in rectangle   (Xmin,Xmax,Ymin,Ymax)        
  mobility.Install (nodes); //install the nodes
  
  // Set up network stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IPv4 addresses
  Ipv4AddressHelper address;
  address.SetBase ("192.9.39.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  UdpEchoServerHelper server01(10);

  ApplicationContainer serverApps01 = server01.Install (nodes.Get (0));
  serverApps01.Start (Seconds (1.0));
  serverApps01.Stop (Seconds (100.0));

  UdpEchoClientHelper client01 (interfaces.GetAddress (0), 10);
  client01.SetAttribute ("MaxPackets", UintegerValue (20));
  client01.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
  client01.SetAttribute ("PacketSize", UintegerValue (1024));
  
  ApplicationContainer clientApps01 = client01.Install (nodes.Get (1));
  clientApps01.Start (Seconds (2.0));
  clientApps01.Stop (Seconds (100.0));

  UdpEchoServerHelper server12(21);

  ApplicationContainer serverApps12 = server12.Install (nodes.Get (1));
  serverApps12.Start (Seconds (1.0));
  serverApps12.Stop (Seconds (100.0));

  UdpEchoClientHelper client12 (interfaces.GetAddress (1), 21);
  client12.SetAttribute ("MaxPackets", UintegerValue (20));
  client12.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
  client12.SetAttribute ("PacketSize", UintegerValue (1024));
  
  ApplicationContainer clientApps12 = client12.Install (nodes.Get (2));
  clientApps12.Start (Seconds (2.0));
  clientApps12.Stop (Seconds (100.0));

    UdpEchoServerHelper server23(27);

  ApplicationContainer serverApps23 = server23.Install (nodes.Get (2));
  serverApps23.Start (Seconds (1.0));
  serverApps23.Stop (Seconds (100.0));

  UdpEchoClientHelper client23 (interfaces.GetAddress (2), 27);
  client23.SetAttribute ("MaxPackets", UintegerValue (20));
  client23.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
  client23.SetAttribute ("PacketSize", UintegerValue (1024));
  
  ApplicationContainer clientApps23 = client23.Install (nodes.Get (3));
  clientApps23.Start (Seconds (2.0));
  clientApps23.Stop (Seconds (100.0));

  UdpEchoServerHelper server34(33);

  ApplicationContainer serverApps34 = server34.Install (nodes.Get (3));
  serverApps34.Start (Seconds (1.0));
  serverApps34.Stop (Seconds (100.0));

  UdpEchoClientHelper client34 (interfaces.GetAddress (3), 33);
  client34.SetAttribute ("MaxPackets", UintegerValue (20));
  client34.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
  client34.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps34 = client34.Install (nodes.Get (4));
  clientApps34.Start (Seconds (2.0));
  clientApps34.Stop (Seconds (100.0));

  UdpEchoServerHelper server45(39);

  ApplicationContainer serverApps45 = server45.Install (nodes.Get (4));
  serverApps45.Start (Seconds (1.0));
  serverApps45.Stop (Seconds (100.0));

  UdpEchoClientHelper client45 (interfaces.GetAddress (4), 39);
  client45.SetAttribute ("MaxPackets", UintegerValue (20));
  client45.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
  client45.SetAttribute ("PacketSize", UintegerValue (1024));
  
  ApplicationContainer clientApps45 = client45.Install (nodes.Get (5));
  clientApps45.Start (Seconds (2.0));
  clientApps45.Stop (Seconds (100.0));

  UdpEchoServerHelper server50(17);

  ApplicationContainer serverApps50 = server50.Install (nodes.Get (5));
  serverApps50.Start (Seconds (1.0));
  serverApps50.Stop (Seconds (100.0));

  UdpEchoClientHelper client50 (interfaces.GetAddress (5), 17);
  client50.SetAttribute ("MaxPackets", UintegerValue (20));
  client50.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
  client50.SetAttribute ("PacketSize", UintegerValue (1024));
  
  ApplicationContainer clientApps50 = client50.Install (nodes.Get (0));
  clientApps50.Start (Seconds (2.0));
  clientApps50.Stop (Seconds (100.0));



  // Define a flow monitor object
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
  Simulator::Stop(Seconds(100.0));
  
  
  // Animation
  AnimationInterface anim("239_task5_6nodes.xml");
  

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
