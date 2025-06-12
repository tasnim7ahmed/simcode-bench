#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("PoissonReverseSimulation");

// A simple routing protocol that uses Poisson Reverse logic
class PoissonReverseRoutingProtocol : public GlobalRouter
{
public:
  PoissonReverseRoutingProtocol () {}
  virtual ~PoissonReverseRoutingProtocol () {}

  static TypeId GetTypeId ()
  {
    static TypeId tid = TypeId ("PoissonReverseRoutingProtocol")
      .SetParent<GlobalRouter> ()
      .SetGroupName("Internet")
      .AddConstructor<PoissonReverseRoutingProtocol> ();
    return tid;
  }

  virtual void PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit) const override
  {
    *stream->GetStream () << "PoissonReverseRoutingTable at time " << Simulator::Now ().As (unit) << std::endl;
    GlobalRouter::PrintRoutingTable (stream, unit);
  }

  void UpdateRoutes ()
  {
    // In a real implementation, this would use the Poisson Reverse algorithm
    // to update the routing table based on distance to destination.
    // For this simple example, we just trigger a route recalculation.
    Ipv4GlobalRoutingHelper::RecomputeRoutingTable (GetIpv4 ());

    NS_LOG_INFO ("Routes updated at time " << Simulator::Now ());

    // Reschedule the next update
    Simulator::Schedule (Seconds (1.0), &PoissonReverseRoutingProtocol::UpdateRoutes, this);
  }
};

int main (int argc, char *argv[])
{
  LogComponent::EnableIlog (LOG_LEVEL_INFO);
  LogComponent::SetLineWidth (120);

  CommandLine cmd;
  cmd.Parse (argc, argv);

  NS_LOG_INFO ("Create nodes.");
  NodeContainer nodes;
  nodes.Create (2);

  NS_LOG_INFO ("Create channel.");
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  NS_LOG_INFO ("Assign IP Addresses.");
  InternetStackHelper stack;
  // Replace the GlobalRouter with our PoissonReverseRoutingProtocol
  stack.ReplaceGlobalRoutingProtocol ("ns3::PoissonReverseRoutingProtocol");
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Populate routing table
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Schedule initial route update for node 0.
  Ptr<Node> node0 = nodes.Get (0);
  Ptr<Ipv4> ipv4_0 = node0->GetObject<Ipv4> ();
  Ptr<PoissonReverseRoutingProtocol> prrp_0 = DynamicCast<PoissonReverseRoutingProtocol>(ipv4_0->GetRoutingProtocol ());
  Simulator::ScheduleWithContext (node0->GetId (), Seconds (1.0), &PoissonReverseRoutingProtocol::UpdateRoutes, prrp_0);

    // Schedule initial route update for node 1.
  Ptr<Node> node1 = nodes.Get (1);
  Ptr<Ipv4> ipv4_1 = node1->GetObject<Ipv4> ();
  Ptr<PoissonReverseRoutingProtocol> prrp_1 = DynamicCast<PoissonReverseRoutingProtocol>(ipv4_1->GetRoutingProtocol ());
  Simulator::ScheduleWithContext (node1->GetId (), Seconds (1.0), &PoissonReverseRoutingProtocol::UpdateRoutes, prrp_1);


  NS_LOG_INFO ("Create Applications.");
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (2.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (3.0));
  clientApps.Stop (Seconds (10.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("poisson-reverse-routing.routes", std::ios::out);
  prrp_0->PrintRoutingTable(routingStream, Seconds);
  prrp_1->PrintRoutingTable(routingStream, Seconds);

  Ptr<Ipv4GlobalRouting> globalRouting = Ipv4GlobalRoutingHelper::GetGlobalRouting ();
  globalRouting->PrintRoutingTableAllAt (Seconds (11.0), routingStream, Seconds);

  int j=0;
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = monitor->GetFlowStats ().begin (); i != monitor->GetFlowStats ().end (); ++i)
    {
      std::cout << "  Flow " << i->first << " (" << i->second.sourceAddress << " -> " << i->second.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024 << " kbps\n";
      j++;
    }

  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  return 0;
}