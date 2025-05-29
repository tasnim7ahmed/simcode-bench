#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/net-device.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PoissonReverseSimulation");

// Minimal Poisson Reverse Routing Protocol
class PoissonReverseRoutingProtocol : public Ipv4RoutingProtocol
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("ns3::PoissonReverseRoutingProtocol")
      .SetParent<Ipv4RoutingProtocol>()
      .SetGroupName("Internet")
      .AddConstructor<PoissonReverseRoutingProtocol>()
      ;
    return tid;
  }

  PoissonReverseRoutingProtocol()
  {
    m_updateInterval = Seconds(1.0);
  }

  virtual ~PoissonReverseRoutingProtocol() {}

  void SetNode(Ptr<Node> node)
  {
    m_node = node;
  }

  void SetInterfaces(std::vector<Ipv4InterfaceAddress> intf)
  {
    m_interfaceAddresses = intf;
  }

  // Periodically update costs (Poisson Reverse logic: use 1/distance)
  void Start()
  {
    Simulator::Schedule(Seconds(0.1), &PoissonReverseRoutingProtocol::PeriodicUpdate, this);
  }

  void PeriodicUpdate()
  {
    // In a simple two-node scenario: Get neighbour and adjust costs
    for (auto & entry : m_interfaceAddresses)
    {
      Ipv4Address myIp = entry.GetLocal();
      // Cost is simulated as inverse of the "distance" (here, hop count == 1)
      double costToDestination = 1.0 / 1.0;

      // Poisson Reverse: Costs invert if a shorter path is found.
      if (costToDestination < m_currentCost)
      {
        m_currentCost = costToDestination;
        NS_LOG_INFO("Node " << m_node->GetId()
                    << " updated route to " << (m_node->GetId() == 0 ? "1" : "0")
                    << " with new cost=" << m_currentCost
                    << " at " << Simulator::Now().GetSeconds() << "s");
      }
    }
    Simulator::Schedule(m_updateInterval, &PoissonReverseRoutingProtocol::PeriodicUpdate, this);
  }

  // ---- Ipv4RoutingProtocol pure virtual methods ---- //
  virtual Ptr<Ipv4Route> RouteOutput (Ptr<Packet> p, const Ipv4Header &header,
                                    Ptr<NetDevice> oif, Socket::SocketErrno &sockerr) override
  {
    Ptr<Ipv4Route> rt = Create<Ipv4Route>();
    // For two node topology, direct route
    Ipv4Address dest = header.GetDestination();
    for (auto & entry : m_interfaceAddresses)
    {
      if (m_node->GetObject<Ipv4>()->GetAddress(1,0).GetLocal() != dest)
      {
        rt->SetSource(m_node->GetObject<Ipv4>()->GetAddress(1,0).GetLocal());
        rt->SetDestination(dest);
        rt->SetGateway(m_node->GetObject<Ipv4>()->GetAddress(1,0).GetLocal()); // no gateway
        rt->SetOutputDevice(m_node->GetDevice(1));
        sockerr = Socket::ERROR_NOTERROR;
        return rt;
      }
    }
    sockerr = Socket::ERROR_NOROUTETOHOST;
    return nullptr;
  }

  virtual bool RouteInput (Ptr<const Packet> p, const Ipv4Header &header,
                          Ptr<const NetDevice> idev,
                          UnicastForwardCallback ucb,
                          MulticastForwardCallback mcb,
                          LocalDeliverCallback lcb,
                          ErrorCallback ecb) override
  {
    uint32_t myIf = m_node->GetObject<Ipv4>()->GetInterfaceForDevice(idev);
    Ipv4Address myAddr = m_node->GetObject<Ipv4>()->GetAddress(myIf,0).GetLocal();
    if (header.GetDestination() == myAddr)
    {
      lcb(p, header, myIf);
      return true;
    }

    for (auto & entry : m_interfaceAddresses)
    {
      // Forward out first interface other than 'idev'
      if (entry.GetLocal() != header.GetSource() && entry.GetLocal() != myAddr)
      {
        Ptr<Ipv4Route> rt = Create<Ipv4Route>();
        rt->SetSource(myAddr);
        rt->SetDestination(header.GetDestination());
        rt->SetGateway(Ipv4Address::GetAny());
        rt->SetOutputDevice(m_node->GetDevice(1));
        ucb(rt, p, header);
        return true;
      }
    }
    ecb(p, header, Socket::ERROR_NOROUTETOHOST);
    return false;
  }

  virtual void NotifyInterfaceUp (uint32_t i) override {}
  virtual void NotifyInterfaceDown (uint32_t i) override {}
  virtual void NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address) override
  {
    m_interfaceAddresses.push_back(address);
  }
  virtual void NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address) override {}

  virtual void SetIpv4 (Ptr<Ipv4> ipv4) override
  {
    m_ipv4 = ipv4;
  }

  virtual void PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit = Time::S) const override
  {
    *stream->GetStream() << "Poisson Reverse Table: node " << m_node->GetId()
      << " cost=" << m_currentCost << std::endl;
  }

private:
  Ptr<Node> m_node;
  Ptr<Ipv4> m_ipv4;
  std::vector<Ipv4InterfaceAddress> m_interfaceAddresses;
  double m_currentCost = 1e6;
  Time m_updateInterval;
};

// Application statistics container
struct AppStats
{
  uint32_t rxPackets = 0;
  uint32_t txPackets = 0;
  uint64_t rxBytes = 0;
  uint64_t txBytes = 0;
};

AppStats stats[2];

void RxCallback(std::string context, Ptr<const Packet> pkt, const Address &)
{
  stats[1].rxPackets++;
  stats[1].rxBytes += pkt->GetSize();
}

void TxCallback(std::string context, Ptr<const Packet> pkt)
{
  stats[0].txPackets++;
  stats[0].txBytes += pkt->GetSize();
}

int main(int argc, char *argv[])
{
  LogComponentEnable("PoissonReverseSimulation", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));
  NetDeviceContainer devices = p2p.Install(nodes);

  InternetStackHelper stack;
  // Do not install default routing; we'll add Poisson Reverse manually later
  stack.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  // Remove default routing protocol and add our Poisson Reverse
  for (uint32_t i = 0; i < nodes.GetN(); ++i)
  {
    Ptr<Node> node = nodes.Get(i);
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    Ptr<Ipv4RoutingProtocol> proto = ipv4->GetRoutingProtocol();
    ipv4->SetRoutingProtocol(0);

    Ptr<PoissonReverseRoutingProtocol> pr = CreateObject<PoissonReverseRoutingProtocol>();
    pr->SetNode(node);

    std::vector<Ipv4InterfaceAddress> intf;
    for (uint32_t j = 0; j < ipv4->GetNInterfaces(); ++j)
    {
      for (uint32_t k = 0; k < ipv4->GetNAddresses(j); ++k)
      {
        intf.push_back(ipv4->GetAddress(j, k));
      }
    }
    pr->SetInterfaces(intf);
    ipv4->SetRoutingProtocol(pr);
    pr->SetIpv4(ipv4);
    pr->Start();
  }

  // Install UDP traffic: node 0 -> node 1
  uint16_t port = 4000;
  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(nodes.Get(1));
  serverApps.Start(Seconds(0.5));
  serverApps.Stop(Seconds(10.0));
  serverApps.Get(0)->TraceConnect("Rx", "", MakeCallback(&RxCallback));

  UdpClientHelper client(interfaces.GetAddress(1), port);
  client.SetAttribute("MaxPackets", UintegerValue(100));
  client.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
  client.SetAttribute("PacketSize", UintegerValue(256));
  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(9.5));
  clientApps.Get(0)->TraceConnect("Tx", "", MakeCallback(&TxCallback));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  NS_LOG_INFO("=== Simulation Statistics ===");
  NS_LOG_INFO("Node 0 TxPackets: " << stats[0].txPackets << ", TxBytes: " << stats[0].txBytes);
  NS_LOG_INFO("Node 1 RxPackets: " << stats[1].rxPackets << ", RxBytes: " << stats[1].rxBytes);

  // Print the routing tables for both nodes
  for (uint32_t i = 0; i < 2; ++i)
  {
    Ptr<OutputStreamWrapper> stream = Create<OutputStreamWrapper>(&std::cout);
    nodes.Get(i)->GetObject<Ipv4>()->GetRoutingProtocol()->PrintRoutingTable(stream);
  }

  Simulator::Destroy();
  return 0;
}