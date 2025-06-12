/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Poisson Reverse Simulation: Point-to-Point network with minimal custom
 * routing protocol that periodically updates route costs based on the inverse
 * of the distance to the destination (Poisson Reverse logic).
 *
 * Traffic flows from node 0 to node 1.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("PoissonReverseSimulation");

// ----------------------------------------------
// Minimal Poisson Reverse Routing Protocol
// ----------------------------------------------

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
    : m_period (Seconds (2.0)),
      m_running (false)
  {
  }

  virtual ~PoissonReverseRouting () {}

  void SetNode (Ptr<Node> node)
  {
    m_node = node;
  }

  virtual Ptr<Ipv4Route> RouteOutput (
    Ptr<Packet> packet, const Ipv4Header &header,
    Ptr<NetDevice> oif, Socket::SocketErrno &sockerr) override
  {
    // Find destination in route table
    Ipv4Address dest = header.GetDestination ();

    auto it = m_routes.find (dest);
    if (it != m_routes.end ())
      {
        Ptr<Ipv4Route> route = Create<Ipv4Route> ();
        route->SetDestination (dest);
        route->SetGateway (it->second.nextHop);
        route->SetSource (it->second.src);
        route->SetOutputDevice (it->second.dev);
        sockerr = Socket::ERROR_NOTERROR;
        return route;
      }

    sockerr = Socket::ERROR_NOROUTETOHOST;
    return nullptr;
  }

  virtual bool RouteInput (
    Ptr<const Packet> packet, const Ipv4Header &header, Ptr<const NetDevice> idev,
    UnicastForwardCallback ucb, MulticastForwardCallback mcb,
    LocalDeliverCallback lcb, ErrorCallback ecb) override
  {
    NS_LOG_FUNCTION (this);
    Ipv4Address dest = header.GetDestination ();
    Ipv4Address src = header.GetSource ();

    // Loopback traffic
    for (uint32_t i = 0; i < m_ipv4->GetNInterfaces (); ++i)
      {
        if (m_ipv4->GetAddress (i, 0).GetLocal () == dest)
          {
            lcb (packet, header, 0);
            return true;
          }
      }

    // Find route
    auto it = m_routes.find (dest);
    if (it != m_routes.end () && it->second.dev != idev)
      {
        // Forward
        ucb (it->second.route, packet, header);
        return true;
      }

    ecb (packet, header, Socket::ERROR_NOROUTETOHOST);
    return false;
  }

  virtual void NotifyInterfaceUp (uint32_t interface) override
  {
    NS_LOG_FUNCTION (this << interface);
    if (!m_running)
      {
        m_running = true;
        Simulator::ScheduleNow (&PoissonReverseRouting::UpdateRoutes, this);
      }
  }

  virtual void NotifyInterfaceDown (uint32_t interface) override {}

  virtual void NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address) override
  {
    m_addresses.push_back (address.GetLocal ());
  }

  virtual void NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address) override {}

  virtual void SetIpv4 (Ptr<Ipv4> ipv4) override
  {
    m_ipv4 = ipv4;
  }

  virtual void PrintRoutingTable (Ptr<OutputStreamWrapper> stream) const override
  {
    *stream->GetStream () << "Poisson Reverse Routing Table:\n";
    for (const auto &entry : m_routes)
      {
        *stream->GetStream ()
          << "Dest: " << entry.first
          << " via " << entry.second.nextHop
          << " cost: " << entry.second.cost
          << "\n";
      }
  }

private:
  struct RouteEntry
  {
    Ipv4Address dest;
    Ipv4Address nextHop;
    Ipv4Address src;
    Ptr<NetDevice> dev;
    double cost;
    Ptr<Ipv4Route> route;
  };

  void UpdateRoutes ()
  {
    // For the simple 2-node p2p topology, just update the route cost 
    // according to Poisson Reverse: 1/distance (distance=1 on p2p).
    // For demonstration, we simulate the cost changing randomly.

    if (!m_ipv4)
      return;

    NS_LOG_INFO ("[" << Simulator::Now ().GetSeconds ()
                     << "s] Node " << m_node->GetId () << ": Updating routes...");

    m_routes.clear ();

    for (uint32_t i = 0; i < m_ipv4->GetNInterfaces (); ++i)
      {
        Ipv4Address local = m_ipv4->GetAddress (i, 0).GetLocal ();
        if (local == Ipv4Address::GetLoopback ())
          continue;
        for (auto &addr : m_addresses)
          {
            if (addr != local && addr != Ipv4Address::GetLoopback ())
              {
                // Poisson Reverse Cost: inverse of (distance)
                double base = 1.0;
                double jitter = 0.5 * ((double)std::rand () / RAND_MAX);
                double cost = 1.0 / (base + jitter); // simulate dynamic/reverse updating

                RouteEntry entry;
                entry.dest = addr;
                entry.nextHop = addr; // In p2p, next hop is the remote
                entry.src = local;
                entry.dev = m_ipv4->GetNetDevice (i);
                entry.cost = cost;
                entry.route = Create<Ipv4Route> ();
                entry.route->SetDestination (addr);
                entry.route->SetGateway (addr);
                entry.route->SetSource (local);
                entry.route->SetOutputDevice (entry.dev);

                m_routes[addr] = entry;

                NS_LOG_INFO ("   Updated route: dest " << addr
                                                      << " via " << entry.nextHop
                                                      << " cost=" << entry.cost);
              }
          }
      }

    ++m_updateCount;
    Simulator::Schedule (m_period, &PoissonReverseRouting::UpdateRoutes, this);
  }

  Time m_period;
  std::map<Ipv4Address, RouteEntry> m_routes;
  std::vector<Ipv4Address> m_addresses;
  Ptr<Ipv4> m_ipv4;
  Ptr<Node> m_node;
  bool m_running;
public:
  uint32_t m_updateCount = 0;
};

// ----------------------------------------------
// Main simulation
// ----------------------------------------------

int main (int argc, char *argv[])
{
  LogComponentEnable ("PoissonReverseSimulation", LOG_LEVEL_INFO);

  Time::SetResolution (Time::NS);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Point-to-point channel configs
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices = p2p.Install (nodes);

  // Install Internet stack with custom routing
  InternetStackHelper stack;
  // Disable default routing
  stack.SetRoutingHelper (Ipv4ListRoutingHelper ());
  stack.Install (nodes);

  // Create and aggregate our PoissonReverseRouting
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      Ptr<PoissonReverseRouting> prRouting = CreateObject<PoissonReverseRouting> ();
      Ptr<Ipv4> ipv4 = nodes.Get (i)->GetObject<Ipv4> ();
      ipv4->SetRoutingProtocol (prRouting);
      prRouting->SetNode (nodes.Get (i));
    }

  // Assign IPs
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // UDP traffic from node 0 to node 1
  uint16_t port = 9999;
  UdpServerHelper server (port);
  ApplicationContainer apps = server.Install (nodes.Get (1));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  UdpClientHelper client (interfaces.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (100));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.2)));
  client.SetAttribute ("PacketSize", UintegerValue (512));
  apps = client.Install (nodes.Get (0));
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();

  // Print routing tables and stats
  NS_LOG_UNCOND ("=== Simulation Complete ===");
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      Ptr<Node> node = nodes.Get (i);
      Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
      Ptr<Ipv4RoutingProtocol> proto = ipv4->GetRoutingProtocol ();
      Ptr<PoissonReverseRouting> pr = DynamicCast<PoissonReverseRouting> (proto);
      if (pr)
        {
          std::ostringstream oss;
          Ptr<OutputStreamWrapper> stream = Create<OutputStreamWrapper> (&oss);
          pr->PrintRoutingTable (stream);
          NS_LOG_UNCOND ("Node " << node->GetId () << " Routing Table:\n" << oss.str ());
          NS_LOG_UNCOND ("Node " << node->GetId () << " Number of route updates: " << pr->m_updateCount);
        }
    }
  // Print app stats
  Ptr<UdpServer> udpServer = apps.Get (0)->GetObject<UdpServer> ();
  NS_LOG_UNCOND ("UDP Server received " << (udpServer ? udpServer->GetReceived () : 0) << " packets");

  Simulator::Destroy ();
  return 0;
}