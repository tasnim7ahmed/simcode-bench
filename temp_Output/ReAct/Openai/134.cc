#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("PoissonReverseSimulation");

// Minimal Poisson Reverse Routing Protocol Implementation
class PoissonReverseRouting : public Ipv4RoutingProtocol
{
public:
  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("PoissonReverseRouting")
      .SetParent<Ipv4RoutingProtocol> ()
      .SetGroupName ("Internet")
      .AddConstructor<PoissonReverseRouting> ();
    return tid;
  }

  PoissonReverseRouting ()
    : m_ipv4 (nullptr)
  {
    m_rng = CreateObject<ExponentialRandomVariable> ();
    m_updateInterval = Seconds (1.0);
  }
  virtual ~PoissonReverseRouting ()
  {
  }

  void SetInterface (Ptr<Ipv4> ipv4)
  {
    m_ipv4 = ipv4;
  }

  void DoInitialize () override
  {
    Simulator::Schedule (Seconds (0.1), &PoissonReverseRouting::PeriodicRouteUpdate, this);
    Ipv4RoutingProtocol::DoInitialize ();
  }

  Ptr<Ipv4Route> RouteOutput (
      Ptr<Packet> p,
      const Ipv4Header &header,
      Ptr<NetDevice> oif,
      Socket::SocketErrno &sockerr) override
  {
    Ipv4Address dst = header.GetDestination ();

    auto it = m_routes.find (dst);
    if (it != m_routes.end ())
    {
      Ptr<Ipv4Route> route = Create<Ipv4Route> ();
      route->SetDestination (dst);
      route->SetSource (m_ipv4->GetAddress (1, 0).GetLocal ());
      route->SetGateway (it->second.nextHop);
      route->SetOutputDevice (m_ipv4->GetNetDevice (it->second.interface));
      sockerr = Socket::ERROR_NOTERROR;
      return route;
    }
    sockerr = Socket::ERROR_NOROUTETOHOST;
    return nullptr;
  }

  bool RouteInput (
      Ptr<const Packet> p,
      const Ipv4Header &header,
      Ptr<const NetDevice> idev,
      UnicastForwardCallback ucb,
      MulticastForwardCallback mcb,
      LocalDeliverCallback lcb,
      ErrorCallback ecb) override
  {
    Ipv4Address dst = header.GetDestination ();
    uint32_t iif = m_ipv4->GetInterfaceForDevice (idev);

    if (m_ipv4->IsDestinationAddress (dst, iif))
    {
      lcb (p, header, iif);
      return true;
    }
    auto it = m_routes.find (dst);
    if (it != m_routes.end ())
    {
      Ptr<Ipv4Route> route = Create<Ipv4Route> ();
      route->SetDestination (dst);
      route->SetSource (header.GetSource ());
      route->SetGateway (it->second.nextHop);
      route->SetOutputDevice (m_ipv4->GetNetDevice (it->second.interface));
      ucb (route, p, header);
      return true;
    }
    ecb (p, header, Socket::ERROR_NOROUTETOHOST);
    return false;
  }

  void NotifyInterfaceUp (uint32_t interface) override {}
  void NotifyInterfaceDown (uint32_t interface) override {}
  void NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address) override
  {
    if (m_ipv4 == nullptr)
      m_ipv4 = GetObject<Ipv4> ();
    m_addresses.push_back (address.GetLocal ());
  }
  void NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address) override {}
  void SetIpv4 (Ptr<Ipv4> ipv4) override
  {
    m_ipv4 = ipv4;
  }
  void PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit = Time::S) const override
  {
    *stream->GetStream () << "PoissonReverse Routing Table for node " << m_ipv4->GetObject<Node> ()->GetId () << std::endl;
    for (auto const &entry : m_routes)
    {
      *stream->GetStream () << entry.first << " via " << entry.second.nextHop << " if " << entry.second.interface << " cost " << entry.second.cost << std::endl;
    }
  }

private:
  struct RouteEntry
  {
    Ipv4Address nextHop;
    uint32_t interface;
    double cost;
  };

  void PeriodicRouteUpdate ()
  {
    // For point-to-point, cost is simply the hop count inverse (distance=1)
    for (auto &other : m_ipv4->GetObject<Node> ()->GetObject<Ipv4> ()->GetObject<Node> ()->GetNetworkInterfaces ())
    {
      // Only two interfaces really, but we keep generic
      if (m_ipv4->GetNInterfaces () < 2)
        break;

      Ipv4Address myAddr = m_ipv4->GetAddress (1, 0).GetLocal ();
      Ipv4Address remoteAddr = m_ipv4->GetAddress (1, 0).GetBroadcast ();
      Ptr<NetDevice> myDev = m_ipv4->GetNetDevice (1);

      Ptr<Channel> ch = myDev->GetChannel ();
      if (!ch)
        continue;

      // Figure out remote node/iface on the channel
      for (uint32_t j = 0; j < ch->GetNDevices (); ++j)
      {
        Ptr<NetDevice> dev = ch->GetDevice (j);
        if (dev == myDev)
          continue;
        Ptr<Node> remoteNode = dev->GetNode ();
        Ptr<Ipv4> remoteIpv4 = remoteNode->GetObject<Ipv4> ();
        Ipv4Address remoteIfAddr = remoteIpv4->GetAddress (1, 0).GetLocal ();

        // For the Poisson Reverse: Next hop is always the neighbor on point-to-point
        double cost = 1.0; // physical hop distance
        double pr_cost = 1.0 / cost; // inverse of distance

        RouteEntry e;
        e.nextHop = remoteIfAddr;
        e.interface = 1;
        e.cost = pr_cost;

        m_routes[remoteIfAddr] = e;

        NS_LOG_INFO ("[PRR] Node " << m_ipv4->GetObject<Node> ()->GetId ()
            << " updates route to " << remoteIfAddr
            << " via " << e.nextHop
            << " cost: " << e.cost);
      }
    }
    ++m_updateCount;
    Simulator::Schedule (m_updateInterval + Seconds (m_rng->GetValue ()), &PoissonReverseRouting::PeriodicRouteUpdate, this);

    if (Simulator::Now ().GetSeconds () > m_runtime)
    {
      NS_LOG_UNCOND ("Node " << m_ipv4->GetObject<Node> ()->GetId ()
          << " completed " << m_updateCount << " route updates.");
    }
  }

  std::map<Ipv4Address, RouteEntry> m_routes;
  std::vector<Ipv4Address> m_addresses;
  Ptr<Ipv4> m_ipv4;
  Time m_updateInterval;
  Ptr<ExponentialRandomVariable> m_rng;
  mutable uint32_t m_updateCount = 0;
  static inline double m_runtime = 10.0;
};

class PoissonReverseRoutingHelper : public Ipv4RoutingHelper
{
public:
  PoissonReverseRoutingHelper () {}
  virtual ~PoissonReverseRoutingHelper () {}

  static PoissonReverseRoutingHelper* Copy (const PoissonReverseRoutingHelper* helper)
  {
    return new PoissonReverseRoutingHelper (*helper);
  }

  virtual Ptr<Ipv4RoutingProtocol> Create (Ptr<Node> node) const override
  {
    Ptr<PoissonReverseRouting> proto = CreateObject<PoissonReverseRouting> ();
    node->AggregateObject (proto);
    return proto;
  }
};

int
main (int argc, char *argv[])
{
  double simTime = 10.0;
  LogComponentEnable ("PoissonReverseSimulation", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices = p2p.Install (nodes);

  InternetStackHelper internet;
  PoissonReverseRoutingHelper prHelper;
  internet.SetRoutingHelper (prHelper);
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.0.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Install UDP traffic from Node 0 to Node 1
  uint16_t port = 4000;
  OnOffHelper onoff ("ns3::UdpSocketFactory", Address (InetSocketAddress (interfaces.GetAddress (1), port)));
  onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("1Mbps")));
  onoff.SetAttribute ("PacketSize", UintegerValue (512));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (simTime-1)));

  ApplicationContainer app = onoff.Install (nodes.Get (0));

  PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (interfaces.GetAddress (1), port));
  ApplicationContainer sinkApp = sink.Install (nodes.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simTime));

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  // Logging final statistics
  Ptr<PacketSink> ps = DynamicCast<PacketSink> (sinkApp.Get (0));
  if (ps)
  {
    NS_LOG_UNCOND ("Total Bytes Received: " << ps->GetTotalRx ());
  }

  Simulator::Destroy ();
  return 0;
}