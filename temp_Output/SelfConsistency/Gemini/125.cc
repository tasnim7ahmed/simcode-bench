#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/rip.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RipStarTopology");

int main (int argc, char *argv[])
{
  LogComponent::SetLogLevel (Rip::Helper::LOG_PREFIX, LOG_LEVEL_ALL);

  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Create Nodes
  NodeContainer nodes;
  nodes.Create (5);

  NodeContainer hubNode = NodeContainer (nodes.Get (0));
  NodeContainer spokeNodes;
  spokeNodes.Add (nodes.Get (1));
  spokeNodes.Add (nodes.Get (2));
  spokeNodes.Add (nodes.Get (3));
  spokeNodes.Add (nodes.Get (4));

  // Create channel between hub and spokes
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer hubDevices;
  NetDeviceContainer spokeDevices;

  for (int i = 0; i < 4; ++i)
    {
      NodeContainer link;
      link.Add (hubNode.Get (0));
      link.Add (spokeNodes.Get (i));
      NetDeviceContainer devices = p2p.Install (link);
      hubDevices.Add (devices.Get (0));
      spokeDevices.Add (devices.Get (1));
    }

  // Install Internet Stack
  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IP Addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer hubInterfaces;
  for (uint32_t i = 0; i < 4; ++i)
    {
      ipv4.Assign (NetDeviceContainer (hubDevices.Get (i)));
      hubInterfaces.Add (ipv4.GetAddress (0));
    }

  ipv4.SetBase ("10.1.1.0", "255.255.255.0"); // Reset base for spoke addresses
  ipv4.NewNetwork ();

  Ipv4InterfaceContainer spokeInterfaces;
  for (uint32_t i = 0; i < 4; ++i)
    {
      ipv4.Assign (NetDeviceContainer (spokeDevices.Get (i)));
      spokeInterfaces.Add (ipv4.GetAddress (0));
      ipv4.NewNetwork (); // New subnet for each spoke
    }


  // Install RIP
  RipHelper ripRouting;
  ripRouting.ExcludeInterface (hubNode.Get (0), hubInterfaces.Get (0)); // Exclude the first hub interface, which is on subnet 10.1.1.0/24
  Address multicastGroup ("224.0.0.9");
  ripRouting.SetProtocolMulticast (multicastGroup);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Create sink application on spoke node 1
  uint16_t port = 9;
  PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = sink.Install (spokeNodes.Get (0));
  sinkApp.Start (Seconds (1.0));
  sinkApp.Stop (Seconds (10.0));

  // Create OnOff application to send traffic from spoke node 2 to spoke node 1
  OnOffHelper onoff ("ns3::UdpSocketFactory", InetSocketAddress (spokeInterfaces.Get (0), port));
  onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute ("DataRate", StringValue ("1Mbps"));
  ApplicationContainer onoffApp = onoff.Install (spokeNodes.Get (1));
  onoffApp.Start (Seconds (2.0));
  onoffApp.Stop (Seconds (9.0));

  // Tracing
  AsciiTraceHelper ascii;
  p2p.EnableAsciiAll (ascii.CreateFileStream ("rip-star.tr"));
  p2p.EnablePcapAll ("rip-star");

  // Animation
  AnimationInterface anim ("rip-star.xml");
  anim.SetConstantPosition (nodes.Get (0), 10, 10);
  anim.SetConstantPosition (nodes.Get (1), 20, 20);
  anim.SetConstantPosition (nodes.Get (2), 20, 0);
  anim.SetConstantPosition (nodes.Get (3), 0, 20);
  anim.SetConstantPosition (nodes.Get (4), 0, 0);

  // Run Simulation
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}