#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RingTopologySimulation");

int main (int argc, char *argv[])
{
  LogComponentEnable ("RingTopologySimulation", LOG_LEVEL_INFO);
  LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);

  // Create 3 nodes
  NodeContainer nodes;
  nodes.Create (3);

  // Create point-to-point links and device containers
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devA_B = p2p.Install (nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devB_C = p2p.Install (nodes.Get(1), nodes.Get(2));
  NetDeviceContainer devC_A = p2p.Install (nodes.Get(2), nodes.Get(0));

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifaceA_B = address.Assign (devA_B);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ifaceB_C = address.Assign (devB_C);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer ifaceC_A = address.Assign (devC_A);

  // Populate routing tables
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Create applications
  uint16_t port = 9;

  // Node 0 sends to Node 1
  OnOffHelper onoff1 ("ns3::UdpSocketFactory",
                      InetSocketAddress (ifaceA_B.GetAddress (1), port));
  onoff1.SetAttribute ("DataRate", StringValue ("1Mbps"));
  onoff1.SetAttribute ("PacketSize", UintegerValue (1024));
  onoff1.SetAttribute ("StartTime", TimeValue(Seconds (1.0)));
  onoff1.SetAttribute ("StopTime", TimeValue(Seconds (8.0)));

  onoff1.Install (nodes.Get(0));

  PacketSinkHelper sink1 ("ns3::UdpSocketFactory",
                          InetSocketAddress (Ipv4Address::GetAny (), port));
  sink1.Install (nodes.Get(1));

  // Node 1 sends to Node 2
  OnOffHelper onoff2 ("ns3::UdpSocketFactory",
                      InetSocketAddress (ifaceB_C.GetAddress (1), port));
  onoff2.SetAttribute ("DataRate", StringValue ("1Mbps"));
  onoff2.SetAttribute ("PacketSize", UintegerValue (1024));
  onoff2.SetAttribute ("StartTime", TimeValue(Seconds (2.0)));
  onoff2.SetAttribute ("StopTime", TimeValue(Seconds (8.0)));

  onoff2.Install (nodes.Get(1));

  PacketSinkHelper sink2 ("ns3::UdpSocketFactory",
                          InetSocketAddress (Ipv4Address::GetAny (), port));
  sink2.Install (nodes.Get(2));

  // Node 2 sends to Node 0 (completing the ring)
  OnOffHelper onoff3 ("ns3::UdpSocketFactory",
                      InetSocketAddress (ifaceC_A.GetAddress (1), port));
  onoff3.SetAttribute ("DataRate", StringValue ("1Mbps"));
  onoff3.SetAttribute ("PacketSize", UintegerValue (1024));
  onoff3.SetAttribute ("StartTime", TimeValue(Seconds (3.0)));
  onoff3.SetAttribute ("StopTime", TimeValue(Seconds (8.0)));

  onoff3.Install (nodes.Get(2));

  PacketSinkHelper sink3 ("ns3::UdpSocketFactory",
                          InetSocketAddress (Ipv4Address::GetAny (), port));
  sink3.Install (nodes.Get(0));

  // Enable PCAP tracing
  p2p.EnablePcapAll ("ring-topology");

  // Run simulation
  Simulator::Stop (Seconds(10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}