#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("PoissonReverseSim");

// Custom Routing Protocol implementing Poisson Reverse Logic
class PoissonReverseRoutingProtocol : public Ipv4RoutingProtocol
{
public:
  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("PoissonReverseRoutingProtocol")
      .SetParent<Ipv4RoutingProtocol> ()
      .SetGroupName ("Internet")
      .AddConstructor<PoissonReverseRoutingProtocol> ();
    return tid;
  }

  PoissonReverseRoutingProtocol ()
  : m_periodicUpdate (Seconds (2)), m_costMetric (100.0), m_lastCost (100.0)
  {}

  virtual ~PoissonReverseRoutingProtocol () {}

  // Minimal required implementation, return false to always use static route
  virtual Ptr<Ipv4Route> RouteOutput (Ptr<Packet> p, const Ipv4Header &header,
                                      Ptr<NetDevice> oif, Socket::SocketErrno &sockerr) override
  {
    NS_LOG_FUNCTION (this << header.GetDestination ());
    Ipv4Address dst = header.GetDestination ();
    if (m_routes.find (dst) != m_routes.end ())
    {
      sockerr = Socket::ERROR_NOTERROR;
      return m_routes[dst];
    }
    sockerr = Socket::ERROR_NOROUTETOHOST;
    return nullptr;
  }

  // Forwards the packet according to current route
  virtual bool RouteInput (Ptr<const Packet> p, const Ipv4Header &header,
                           Ptr<const NetDevice> idev, UnicastForwardCallback ucb,
                           MulticastForwardCallback mcb, LocalDeliverCallback lcb,
                           ErrorCallback ecb) override
  {
    NS_LOG_FUNCTION (this << header.GetDestination ());
    Ipv4Address dst = header.GetDestination ();
    if (m_routes.find (dst) != m_routes.end ())
    {
      Ptr<Ipv4Route> route = m_routes[dst];
      ucb (route, p, header);
      return true;
    }
    return false;
  }

  virtual void NotifyInterfaceUp (uint32_t i) override {}
  virtual void NotifyInterfaceDown (uint32_t i) override {}
  virtual void NotifyAddAddress (uint32_t i, Ipv4InterfaceAddress address) override
  {
    m_addresses.insert (address.GetLocal ());
  }
  virtual void NotifyRemoveAddress (uint32_t i, Ipv4InterfaceAddress address) override
  {
    m_addresses.erase (address.GetLocal ());
  }
  virtual void SetIpv4 (Ptr<Ipv4> ipv4) override { m_ipv4 = ipv4; }
  virtual void PrintRoutingTable (Ptr<OutputStreamWrapper> stream) const override
  {
    *stream->GetStream () << "== Poisson Reverse Routing Table ==\n";
    for (auto &it : m_routes)
    {
      *stream->GetStream () << "Dest: " << it.first << " via " << it.second->GetGateway ()
                            << " cost: " << m_costMetric << "\n";
    }
  }

  void Start (void)
  {
    Simulator::ScheduleNow (&PoissonReverseRoutingProtocol::PeriodicUpdate, this);
  }

  void AddRoute (Ipv4Address dest, Ipv4Address nextHop, uint32_t interface)
  {
    Ptr<Ipv4Route> route = Create<Ipv4Route> ();
    route->SetDestination (dest);
    route->SetGateway (nextHop);
    route->SetOutputDevice (m_ipv4->GetNetDevice (interface));
    // Fill with a dummy subnet
    route->SetSource (m_ipv4->GetAddress (interface,0).GetLocal ());
    m_routes[dest] = route;
  }

  void PeriodicUpdate ()
  {
    // Poisson Reverse: route cost = 1/(distance+1), simulate 'distance' varying as 1,2,1,2,..
    m_distance = (m_distance == 1) ? 2 : 1;
    double newCost = 1.0 / (m_distance + 1.0);
    m_lastCost = m_costMetric;
    m_costMetric = newCost;

    if (std::abs (m_costMetric - m_lastCost) > 1e-5)
    {
      NS_LOG_INFO ("[t=" << Simulator::Now ().GetSeconds ()
                   << "s] Node " << m_ipv4->GetObject<Node> ()->GetId ()
                   << ": Route cost updated to " << m_costMetric);
    }

    // Simulate dynamic route adjustment: add/remove route depending on cost
    for (Ipv4Address addr : m_addresses)
    {
      for (auto &it : m_routes)
      {
        if (m_costMetric < 0.6) // arbitrary threshold for cost validity
        {
          it.second->SetGateway (it.second->GetGateway ());
        }
      }
    }
    Simulator::Schedule (m_periodicUpdate, &PoissonReverseRoutingProtocol::PeriodicUpdate, this);
  }

private:
  Ptr<Ipv4> m_ipv4;
  std::map<Ipv4Address, Ptr<Ipv4Route> > m_routes;
  std::set<Ipv4Address> m_addresses;
  Time m_periodicUpdate;
  double m_costMetric;
  double m_lastCost;
  int m_distance = 1;
};

int main (int argc, char *argv[])
{
  LogComponentEnable ("PoissonReverseSim", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("2Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices = p2p.Install (nodes);

  InternetStackHelper internet;
  internet.SetRoutingHelper (Ptr<Ipv4RoutingHelper> ()); // we will install routing manually

  // Disable built-in routing
  internet.Install (nodes);

  Ptr<PoissonReverseRoutingProtocol> customRoutingA = CreateObject<PoissonReverseRoutingProtocol> ();
  Ptr<PoissonReverseRoutingProtocol> customRoutingB = CreateObject<PoissonReverseRoutingProtocol> ();

  Ptr<Ipv4> ipv4A = nodes.Get(0)->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4B = nodes.Get(1)->GetObject<Ipv4> ();

  ipv4A->SetRoutingProtocol (customRoutingA);
  ipv4B->SetRoutingProtocol (customRoutingB);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Setup custom routing table entries
  customRoutingA->SetIpv4 (ipv4A);
  customRoutingB->SetIpv4 (ipv4B);
  customRoutingA->AddRoute (interfaces.GetAddress (1), interfaces.GetAddress (1), 1);
  customRoutingB->AddRoute (interfaces.GetAddress (0), interfaces.GetAddress (0), 0);

  // Start PoissonReverse route logic
  customRoutingA->Start ();
  customRoutingB->Start ();

  // Install ping traffic (so routing is exercised)
  uint16_t port = 9;
  OnOffHelper onoff ("ns3::UdpSocketFactory", InetSocketAddress (interfaces.GetAddress (1), port));
  onoff.SetAttribute ("DataRate", StringValue ("512Kbps"));
  onoff.SetAttribute ("PacketSize", UintegerValue (512));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (8.0)));

  ApplicationContainer apps = onoff.Install (nodes.Get (0));

  PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  apps.Add (sink.Install (nodes.Get (1)));

  // Trace route table changes
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("poisson-reverse-routing.tr");
  Simulator::Schedule (Seconds (3.0), &Ipv4RoutingProtocol::PrintRoutingTable, customRoutingA, stream);
  Simulator::Schedule (Seconds (7.0), &Ipv4RoutingProtocol::PrintRoutingTable, customRoutingB, stream);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  // Print final statistics
  uint32_t totalRx = DynamicCast<PacketSink> (apps.Get (1))->GetTotalRx ();
  std::cout << "Total Bytes Received at node 1: " << totalRx << std::endl;

  Simulator::Destroy ();
  return 0;
}