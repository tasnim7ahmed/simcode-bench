#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <iostream>
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("PoissonReverseRouting");

class PoissonReverseRoutingProtocol : public Ipv4RoutingProtocol
{
public:
  PoissonReverseRoutingProtocol ();
  virtual ~PoissonReverseRoutingProtocol ();

  static TypeId GetTypeId (void);
  virtual void PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit timeUnit) const;

  virtual Ptr<Ipv4Route> RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::ErrorCallback errorCallback) override;
  virtual bool RouteInput (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev, UnicastForwardCallback ucb, MulticastForwardCallback mcb, LocalDeliverCallback ldb, ErrorCallback ecb) override;

private:
  void DoDispose () override;
  void StartUpdateTimer ();
  void UpdateRoutes ();
  double CalculateRouteCost (Ipv4Address dest);

  EventId m_updateTimer;
  Time m_updateInterval;
  Ipv4InterfaceAddress m_ipv4Interface;
  Ipv4Address m_destination;
  double m_currentCost;
};

NS_OBJECT_ENSURE_REGISTERED (PoissonReverseRoutingProtocol);

PoissonReverseRoutingProtocol::PoissonReverseRoutingProtocol ()
  : m_updateInterval (Seconds (1))
{
  NS_LOG_FUNCTION (this);
  m_currentCost = 1.0;
}

PoissonReverseRoutingProtocol::~PoissonReverseRoutingProtocol ()
{
  NS_LOG_FUNCTION (this);
  Cancel ();
}

TypeId
PoissonReverseRoutingProtocol::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PoissonReverseRoutingProtocol")
    .SetParent<Ipv4RoutingProtocol> ()
    .SetGroupName("Internet")
    .AddConstructor<PoissonReverseRoutingProtocol> ()
    ;
  return tid;
}

void
PoissonReverseRoutingProtocol::PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit timeUnit) const
{
  *stream << "PoissonReverseRouting Table at time " << Simulator::Now ().As (timeUnit) << std::endl;
  *stream << "Destination: " << m_destination << ", Cost: " << m_currentCost << std::endl;
}

Ptr<Ipv4Route>
PoissonReverseRoutingProtocol::RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::ErrorCallback errorCallback)
{
  NS_LOG_FUNCTION (this << p << header << oif << errorCallback);

  if (m_destination == Ipv4Address ("0.0.0.0"))
    {
      NS_LOG_LOGIC ("No route to destination");
      return nullptr;
    }

  Ptr<Ipv4Route> route = Create<Ipv4Route> ();
  route->SetDestination (m_destination);
  route->SetGateway (m_destination);
  route->SetOutputDevice (oif);
  route->SetSource (m_ipv4Interface.GetLocal ());
  route->SetMetric(m_currentCost);
  return route;
}

bool
PoissonReverseRoutingProtocol::RouteInput (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev, UnicastForwardCallback ucb, MulticastForwardCallback mcb, LocalDeliverCallback ldb, ErrorCallback ecb)
{
  NS_LOG_FUNCTION (this << p << header << idev << ucb << mcb << ldb << ecb);
  if (m_ipv4Interface.GetLocal () == header.GetDestination ())
    {
      NS_LOG_LOGIC ("This is for me");
      ldb (p, header, idev);
      return true;
    }
  else
    {
      NS_LOG_LOGIC ("Forwarding packet");
      if (!ucb (p, header, m_destination))
        {
          ecb (p, header, Socket::ERROR_NOROUTETOHOST);
          return false;
        }
      return true;
    }
}

void
PoissonReverseRoutingProtocol::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  Cancel ();
  Ipv4RoutingProtocol::DoDispose ();
}

void
PoissonReverseRoutingProtocol::StartUpdateTimer ()
{
  NS_LOG_FUNCTION (this);
  m_updateTimer = Simulator::Schedule (m_updateInterval, &PoissonReverseRoutingProtocol::UpdateRoutes, this);
}

void
PoissonReverseRoutingProtocol::UpdateRoutes ()
{
  NS_LOG_FUNCTION (this);
  m_currentCost = CalculateRouteCost (m_destination);
  NS_LOG_INFO ("Updated route cost to " << m_currentCost << " for destination " << m_destination);
  StartUpdateTimer ();
}

double
PoissonReverseRoutingProtocol::CalculateRouteCost (Ipv4Address dest)
{
  NS_LOG_FUNCTION (this << dest);
  double distance = 1.0;
  if (dest != Ipv4Address ("0.0.0.0")) {
    distance = 1.0/(dest.Get () + 1.0);
  }
  return distance;
}

void
PoissonReverseRoutingProtocol::SetIpv4 (Ptr<Ipv4> ipv4)
{
  NS_LOG_FUNCTION (this << ipv4);
  m_ipv4 = ipv4;
  m_ipv4Interface = ipv4->GetAddress (1, 0);
  m_destination = Ipv4Address ("10.1.1.2");

  StartUpdateTimer ();
}

void
PoissonReverseRoutingProtocol::Cancel ()
{
  NS_LOG_FUNCTION (this);
  Simulator::Cancel (m_updateTimer);
}

int
main (int argc, char *argv[])
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

  TypeId pid = TypeId::LookupByName ("ns3::PoissonReverseRoutingProtocol");
  Ptr<PoissonReverseRoutingProtocol> routingProtocol = CreateObject<PoissonReverseRoutingProtocol> ();
  Ptr<Ipv4> ipv4_node0 = nodes.Get (0)->GetObject<Ipv4> ();
  ipv4_node0->SetRoutingProtocol (routingProtocol);
  routingProtocol->SetIpv4(ipv4_node0);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

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

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (11.0));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("poisson-reverse-routing.routes", std::ios::out);
  routingProtocol->PrintRoutingTable (routingStream, Seconds);

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024 / 1024  << " Mbps\n";
    }

  monitor->SerializeToXmlFile("poisson-reverse-routing.flowmon", true, true);

  Simulator::Destroy ();

  return 0;
}