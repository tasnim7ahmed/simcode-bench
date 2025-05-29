#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/random-variable-stream.h"
#include "ns3/uinteger.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("PoissonReverse");

class PoissonReverseRouting : public GlobalRouteManagerImpl
{
public:
  PoissonReverseRouting ();
  virtual ~PoissonReverseRouting ();

  static TypeId GetTypeId (void);

  void UpdateRouteCosts ();

private:
  virtual void DoDispose (void);
  void Initialize ();

  Time m_updateInterval;
  EventId m_updateEvent;
};

NS_OBJECT_ENSURE_REGISTERED (PoissonReverseRouting);

TypeId
PoissonReverseRouting::GetTypeId (void)
{
  static TypeId tid = TypeId ("PoissonReverseRouting")
    .SetParent<GlobalRouteManagerImpl> ()
    .SetGroupName("Internet")
    .AddConstructor<PoissonReverseRouting> ()
    .AddAttribute ("UpdateInterval",
                   "The interval between route cost updates.",
                   TimeValue (Seconds (1.0)),
                   MakeTimeAccessor (&PoissonReverseRouting::m_updateInterval),
                   MakeTimeChecker ());
  return tid;
}

PoissonReverseRouting::PoissonReverseRouting () : m_updateInterval (Seconds(1.0))
{
  NS_LOG_FUNCTION (this);
}

PoissonReverseRouting::~PoissonReverseRouting ()
{
  NS_LOG_FUNCTION (this);
}

void
PoissonReverseRouting::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  GlobalRouteManagerImpl::DoDispose ();
}

void
PoissonReverseRouting::Initialize ()
{
  NS_LOG_FUNCTION (this);
  GlobalRouteManagerImpl::Initialize ();

  m_updateEvent = Simulator::ScheduleNow (&PoissonReverseRouting::UpdateRouteCosts, this);
}

void
PoissonReverseRouting::UpdateRouteCosts ()
{
  NS_LOG_FUNCTION (this);

  Ptr<Ipv4GlobalRouting> routing = GetGlobalRouting ();
  if (routing == 0)
    {
      NS_LOG_WARN ("Global routing not enabled.");
      return;
    }

  Ipv4RoutingTable& table = routing->GetRoutingTable ();

  for (Ipv4RoutingTableEntry& entry : table)
    {
      Ipv4Address dest = entry.GetDest ();
      uint32_t interface = entry.GetOutputTtl ();

      if (dest != Ipv4Address::GetZero () && interface != 0)
        {
          double distance = CalculateDistance(GetIpv4 (), dest);
          double cost = 1.0 / (distance + 0.00001);

          NS_LOG_INFO("Updating route to " << dest << " with cost " << cost);
          entry.SetCost (cost);
        }
    }

  m_updateEvent = Simulator::Schedule (m_updateInterval, &PoissonReverseRouting::UpdateRouteCosts, this);
}

int main (int argc, char *argv[])
{
  LogComponentEnable ("PoissonReverse", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

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

  GlobalRouteManager::PopulateRoutingTables ();
  Ptr<PoissonReverseRouting> poissonReverseRouting = CreateObject<PoissonReverseRouting> ();
  poissonReverseRouting->Initialize ();

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

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps\n";
    }

  Simulator::Destroy ();

  return 0;
}