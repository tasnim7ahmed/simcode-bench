#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TreeTopologySimulation");

int main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("TreeTopologySimulation", LOG_LEVEL_INFO);

  NS_LOG_INFO ("Creating nodes.");
  NodeContainer root;
  root.Create (1);

  NodeContainer childNodes;
  childNodes.Create (2);

  NodeContainer leafNode;
  leafNode.Create (1);

  NS_LOG_INFO ("Creating point-to-point links.");
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer rootToChild1 = p2p.Install (root.Get (0), childNodes.Get (0));
  NetDeviceContainer rootToChild2 = p2p.Install (root.Get (0), childNodes.Get (1));
  NetDeviceContainer child1ToLeaf = p2p.Install (childNodes.Get (0), leafNode.Get (0));

  NS_LOG_INFO ("Installing internet stack.");
  InternetStackHelper stack;
  stack.Install (root);
  stack.Install (childNodes);
  stack.Install (leafNode);

  NS_LOG_INFO ("Assigning IP addresses.");
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer rootToChild1Interfaces = address.Assign (rootToChild1);
  address.NewNetwork ();
  Ipv4InterfaceContainer rootToChild2Interfaces = address.Assign (rootToChild2);
  address.NewNetwork ();
  Ipv4InterfaceContainer child1ToLeafInterfaces = address.Assign (child1ToLeaf);

  NS_LOG_INFO ("Setting up applications.");
  uint16_t port = 9;
  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (root.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (rootToChild1Interfaces.GetAddress (0), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (leafNode.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  NS_LOG_INFO ("Enabling pcap tracing.");
  p2p.EnablePcapAll ("tree_topology_simulation");

  NS_LOG_INFO ("Running simulation.");
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}