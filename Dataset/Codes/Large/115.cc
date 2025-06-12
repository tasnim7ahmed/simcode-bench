#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

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
  

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());
  //phy.Set("TxPowerStart", DoubleValue(10.0));
  //phy.Set("TxPowerEnd", DoubleValue(10.0));
  //phy.Set("TxGain", DoubleValue(0.0));
  //phy.Set("RxGain", DoubleValue(0.0));

  

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);
  

  // Set up mobility model
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (5.0),
                                 "GridWidth", UintegerValue (2),
                                 "LayoutType", StringValue ("RowFirst"));
                                 
                                
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds", RectangleValue(Rectangle(-25, 25, -25, 25)));
  mobility.Install (nodes);
  

  // Set up network stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IPv4 addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Create a UDP echo server application on node 1
  uint16_t port = 9; // well-known echo port number
  UdpEchoServerHelper server (port);
  ApplicationContainer serverApps = server.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (100.0));

  // Create a UDP echo client application on node 0
  UdpEchoClientHelper client (interfaces.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (1));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  
  ApplicationContainer clientApps = client.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (100.0));
  
  // Define a flow monitor object
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll();
 
  
  
  Simulator::Stop(Seconds(100.0));
  
  
  // Animation
  AnimationInterface anim("two_wireless_nodes.xml");
  

  // Run simulation
  Simulator::Run ();
  
  // Calculate packet delivery ratio and end to end delay and throughput
  uint32_t packetsDelivered = 0;
  uint32_t totalPackets = 0;
  double totalDelay = 0;
  
  
  // Retrive the flow statistics
  FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
  for(auto & it: stats)
  {
  	FlowId flowId = it.first;
    	std::cout << "Flow " << flowId << std::endl;
    	std::cout << " Tx Packets: " << it.second.txPackets << std::endl;
    	std::cout << " Tx Bytes: " << it.second.txBytes << std::endl;
    	std::cout << " Rx Packets: " << it.second.rxPackets << std::endl;
    	std::cout << " Rx Bytes: " << it.second.rxBytes << std::endl;
    	
  	if(it.second.rxPackets > 0)
  	{
  		packetsDelivered += it.second.rxPackets;
  		totalPackets += it.second.txPackets;
  		totalDelay += (it.second.delaySum.ToDouble(ns3::Time::S) / it.second.rxPackets);
  	}
  }
  
  double packetDeliveryRatio = (double) packetsDelivered / totalPackets;
  double averageEndToEndDelay = totalDelay / packetsDelivered;
  double throughput = (double) packetsDelivered * 1024 * 8 / (100.0 - 0.0) / 1000000;
  
  // Printing the results
  std::cout << "Packets Delivered Ratio: " << packetDeliveryRatio << std::endl;
  std::cout << "Average End-to-End Delay: " << averageEndToEndDelay << std::endl;
  std::cout << "Throughput: " << throughput << std::endl;
  
  Simulator::Destroy ();

  return 0;
}
