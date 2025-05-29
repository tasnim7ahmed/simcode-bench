#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("PoissonReverseRouting");

class PoissonReverseRouting : public GlobalRouteManager
{
public:
  PoissonReverseRouting () {}
  ~PoissonReverseRouting () {}

  static TypeId GetTypeId ()
  {
    static TypeId tid = TypeId ("PoissonReverseRouting")
      .SetParent<GlobalRouteManager> ()
      .SetGroupName("Routing")
      .AddConstructor<PoissonReverseRouting> ();
    return tid;
  }

  virtual void UpdateRoutingTables () override
  {
    NS_LOG_FUNCTION (this);

    Ptr<Node> node0 = NodeList::GetNode (0);
    Ptr<Node> node1 = NodeList::GetNode (1);

    Ipv4Address address0 = node0->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal ();
    Ipv4Address address1 = node1->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal ();

    double distance = CalculateDistance (node0, node1);

    double cost0to1 = 1.0 / distance;
    double cost1to0 = 1.0 / distance;

    Ipv4GlobalRoutingHelper::PopulateRoutingTableEntry (node0, address1, 255, address1, 1, cost0to1);
    Ipv4GlobalRoutingHelper::PopulateRoutingTableEntry (node1, address0, 255, address0, 1, cost1to0);

    NS_LOG_INFO ("Updated routing tables. Distance: " << distance << ", Cost0to1: " << cost0to1 << ", Cost1to0: " << cost1to0);
  }

private:
  double CalculateDistance (Ptr<Node> node1, Ptr<Node> node2)
  {
    Ptr<MobilityModel> mob1 = node1->GetObject<MobilityModel> ();
    Ptr<MobilityModel> mob2 = node2->GetObject<MobilityModel> ();

    Vector pos1 = mob1->GetPosition ();
    Vector pos2 = mob2->GetPosition ();

    return std::sqrt (std::pow (pos1.x - pos2.x, 2) + std::pow (pos1.y - pos2.y, 2) + std::pow (pos1.z - pos2.z, 2));
  }
};

int main (int argc, char *argv[])
{
  LogComponentEnable ("PoissonReverseRouting", LOG_LEVEL_INFO);

  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  Ptr<PoissonReverseRouting> routingProtocol = CreateObject<PoissonReverseRouting> ();
  routingProtocol->UpdateRoutingTables ();

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Schedule (Seconds (5.0), &PoissonReverseRouting::UpdateRoutingTables, routingProtocol);

  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowMonHelper;
  flowMonitor = flowMonHelper.InstallAll ();

  Simulator::Stop (Seconds (11.0));

  AnimationInterface anim ("poisson-reverse.xml");

  Simulator::Run ();

  flowMonitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowMonHelper.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024 / 1024  << " Mbps\n";
    }

  flowMonitor->SerializeToXmlFile("poisson-reverse.flowmon", true, true);

  Simulator::Destroy ();
  return 0;
}