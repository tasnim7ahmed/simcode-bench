#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("PoissonReverseRouting");

class PoissonReverseRoutingProtocol : public RoutingProtocol
{
public:
  static TypeId GetTypeId (void);
  PoissonReverseRoutingProtocol ();
  virtual ~PoissonReverseRoutingProtocol ();

  void SetUpdateInterval (Time interval);
  void SetPoissonParameter (double lambda);

protected:
  virtual void DoDispose (void);
  virtual void Start ();
  void UpdateRoutes ();
  virtual void NotifyInterfaceUp (uint32_t interface);
  virtual void NotifyInterfaceDown (uint32_t interface);

private:
  void ScheduleUpdate ();
  double GetCostTo (Ipv4Address dest);
  void PrintRoutingTable ();

  Time m_updateInterval;
  double m_poissonParameter;
  EventId m_updateEvent;
  Ipv4InterfaceAddress m_address;
  Ipv4RoutingTable m_routingTable;
};

NS_OBJECT_ENSURE_REGISTERED (PoissonReverseRoutingProtocol);

TypeId
PoissonReverseRoutingProtocol::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PoissonReverseRoutingProtocol")
    .SetParent<RoutingProtocol> ()
    .SetGroupName("Routing")
    .AddConstructor<PoissonReverseRoutingProtocol> ()
    .AddAttribute ("UpdateInterval",
                   "The interval between routing updates",
                   TimeValue (Seconds (10.0)),
                   MakeTimeAccessor (&PoissonReverseRoutingProtocol::m_updateInterval),
                   MakeTimeChecker ())
    .AddAttribute ("PoissonParameter",
                   "Lambda parameter for the Poisson distribution",
                   DoubleValue (1.0),
                   MakeDoubleAccessor (&PoissonReverseRoutingProtocol::m_poissonParameter),
                   MakeDoubleChecker<double> (0.0));
  return tid;
}

PoissonReverseRoutingProtocol::PoissonReverseRoutingProtocol ()
  : m_updateInterval (Seconds (10.0)),
    m_poissonParameter (1.0)
{
  NS_LOG_FUNCTION (this);
}

PoissonReverseRoutingProtocol::~PoissonReverseRoutingProtocol ()
{
  NS_LOG_FUNCTION (this);
}

void
PoissonReverseRoutingProtocol::SetUpdateInterval (Time interval)
{
  NS_LOG_FUNCTION (this << interval);
  m_updateInterval = interval;
}

void
PoissonReverseRoutingProtocol::SetPoissonParameter (double lambda)
{
  NS_LOG_FUNCTION (this << lambda);
  m_poissonParameter = lambda;
}

void
PoissonReverseRoutingProtocol::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  CancelEvent (m_updateEvent);
  RoutingProtocol::DoDispose ();
}

void
PoissonReverseRoutingProtocol::Start ()
{
  NS_LOG_FUNCTION (this);
  m_address = m_ipv4->GetAddress (1, 0);
  ScheduleUpdate ();
}

void
PoissonReverseRoutingProtocol::NotifyInterfaceUp (uint32_t interface)
{
  NS_LOG_FUNCTION (this << interface);
}

void
PoissonReverseRoutingProtocol::NotifyInterfaceDown (uint32_t interface)
{
  NS_LOG_FUNCTION (this << interface);
}

void
PoissonReverseRoutingProtocol::UpdateRoutes ()
{
  NS_LOG_FUNCTION (this);

  Ptr<Ipv4> ipv4 = GetObject<Ipv4> ();
  for (uint32_t i = 0; i < ipv4->GetNInterfaces (); ++i)
  {
    Ipv4InterfaceAddress iface = ipv4->GetAddress (i, 0);
    if (iface.GetLocal () == Ipv4Address ("127.0.0.1"))
    {
      continue;
    }

    for (uint32_t j = 0; j < ipv4->GetNInterfaces (); ++j)
    {
      if (i == j)
      {
        continue;
      }

      Ipv4InterfaceAddress destIface = ipv4->GetAddress (j, 0);
      if (destIface.GetLocal () == Ipv4Address ("127.0.0.1"))
      {
        continue;
      }

      Ipv4Address dest = destIface.GetLocal ();
      double cost = GetCostTo (dest);

      Ipv4StaticRoutingHelper::AddRoute (GetNode (), dest, Ipv4Mask ("255.255.255.255"), iface.GetLocal (), cost);

      NS_LOG_INFO ("Route added to " << dest << " via " << iface.GetLocal () << " with cost " << cost);
    }
  }

  PrintRoutingTable ();
  ScheduleUpdate ();
}

double
PoissonReverseRoutingProtocol::GetCostTo (Ipv4Address dest)
{
  NS_LOG_FUNCTION (this << dest);
  double distance = std::sqrt(std::pow(dest.Get () - m_address.GetLocal ().Get (), 2));

  if (distance == 0) {
      return 0;
  }

  double inverseDistance = 1.0 / distance;

  double poissonSample = 0;
  Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();
  uv->SetStream (Time::Now ().GetMicroSeconds ());
  for (int i = 0; i < 100; ++i) {
      double u = uv->GetValue();
      poissonSample += (u < m_poissonParameter * inverseDistance) ? 1 : 0;
  }

  return poissonSample;
}

void
PoissonReverseRoutingProtocol::ScheduleUpdate ()
{
  NS_LOG_FUNCTION (this);
  m_updateEvent = Simulator::Schedule (m_updateInterval, &PoissonReverseRoutingProtocol::UpdateRoutes, this);
}

void
PoissonReverseRoutingProtocol::PrintRoutingTable ()
{
  NS_LOG_FUNCTION (this);

  Ptr<Ipv4StaticRouting> routing = StaticCast<Ipv4StaticRouting>(m_ipv4->GetRoutingProtocol ());
  if (routing)
  {
      std::stringstream ss;
      routing->PrintRoutingTable (ss);
      NS_LOG_INFO ("Routing Table: " << ss.str ());
  } else {
      NS_LOG_WARN ("No static routing protocol found.");
  }
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

  TypeId tid = TypeId::LookupByName ("ns3::PoissonReverseRoutingProtocol");
  GlobalValue::Bind ("RoutingHelper::DefaultRoutingProtocol", TypeIdValue (tid));

  Ptr<PoissonReverseRoutingProtocol> routingProtocol = CreateObject<PoissonReverseRoutingProtocol> ();
  nodes.Get (0)->GetObject<Ipv4>()->SetRoutingProtocol (routingProtocol);
  routingProtocol->Start();

  Ptr<PoissonReverseRoutingProtocol> routingProtocol1 = CreateObject<PoissonReverseRoutingProtocol> ();
  nodes.Get (1)->GetObject<Ipv4>()->SetRoutingProtocol (routingProtocol1);
  routingProtocol1->Start();

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

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}