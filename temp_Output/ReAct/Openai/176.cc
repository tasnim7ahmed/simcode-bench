#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/queue-disc.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UdpPacketLossExample");

int 
main (int argc, char *argv[])
{
  // Simulation parameters
  double simTime = 5.0;
  uint32_t pktSize = 1024; // bytes
  uint32_t numPkts = 1000;
  double pktInterval = 0.002; // seconds = 500pps
  
  // Create nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Point-to-point config
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
  p2p.SetQueue ("ns3::DropTailQueue<Packet>", "MaxPackets", UintegerValue (20));

  // Install devices
  NetDeviceContainer devices = p2p.Install (nodes);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IPs
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // UDP server (sink) on node 1
  uint16_t port = 4000;
  UdpServerHelper server (port);
  ApplicationContainer apps = server.Install (nodes.Get (1));
  apps.Start (Seconds (0.0));
  apps.Stop (Seconds (simTime));

  // UDP client on node 0
  UdpClientHelper client (interfaces.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (numPkts));
  client.SetAttribute ("Interval", TimeValue (Seconds (pktInterval)));
  client.SetAttribute ("PacketSize", UintegerValue (pktSize));

  ApplicationContainer clientApps = client.Install (nodes.Get (0));
  clientApps.Start (Seconds (0.5));
  clientApps.Stop (Seconds (simTime));

  // NetAnim setup
  AnimationInterface anim ("packet-loss-netanim.xml");
  anim.SetConstantPosition (nodes.Get (0), 20.0, 30.0);
  anim.SetConstantPosition (nodes.Get (1), 80.0, 30.0);

  // Trace packet drops at device queue
  devices.Get (0)->GetObject<PointToPointNetDevice> ()->GetQueue ()->TraceConnectWithoutContext (
    "Drop",
    MakeCallback ([] (Ptr<const Packet> packet) {
      NS_LOG_UNCOND ("*** Packet dropped at queue! uid=" << packet->GetUid ());
    })
  );

  // Animate drops: use packet metadata if available (otherwise, just animate a red packet at node 0)
  Ptr<NetDevice> dev = devices.Get (0);
  dev->GetObject<Queue<Packet> >()->TraceConnectWithoutContext (
    "Drop",
    MakeCallback ([&anim] (Ptr<const Packet> packet) {
      anim.UpdateNodeColor (0, 255, 0, 0); // flash source node red
      Simulator::Schedule (MilliSeconds (50), 
        [&anim] () { anim.UpdateNodeColor (0, 200, 200, 200); }); // back to grey
    })
  );

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}