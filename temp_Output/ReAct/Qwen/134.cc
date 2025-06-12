#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/ipv4-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PoissonReverseSimulation");

class PoissonReverseRouting : public Ipv4RoutingProtocol {
public:
  static TypeId GetTypeId() {
    static TypeId tid = TypeId("PoissonReverseRouting")
      .SetParent<Ipv4RoutingProtocol>()
      .AddConstructor<PoissonReverseRouting>();
    return tid;
  }

  PoissonReverseRouting() : m_node(nullptr) {}

  void SetNode(Ptr<Node> node) { m_node = node; }

  Ptr<IpRoute> RouteOutput(Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr) override {
    return nullptr;
  }

  bool RouteInput(Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
                  UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                  LocalDeliverCallback lcb, ErrorCallback ecb) override {
    if (header.GetDestination() == m_node->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal()) {
      lcb(p, header, 0);
      return true;
    }
    ucb(idev, m_ipv4, p, header, header.GetDestination());
    return true;
  }

  void NotifyInterfaceUp(uint32_t interface) override {}
  void NotifyInterfaceDown(uint32_t interface) override {}
  void NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address) override {}
  void NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address) override {}
  void SetIpv4(Ptr<Ipv4> ipv4) override { m_ipv4 = ipv4; }
  void PrintRoutingTable(Ptr<OutputStreamWrapper> stream) const override {}

  void UpdateRoutes() {
    NS_LOG_INFO("Updating routes using Poisson Reverse logic at time " << Simulator::Now().As(Time::S));
    for (uint32_t i = 0; i < m_node->GetNDevices(); ++i) {
      Ptr<PointToPointNetDevice> dev = DynamicCast<PointToPointNetDevice>(m_node->GetDevice(i));
      if (!dev || !dev->GetChannel()) continue;

      double distance = 1.0 / (1.0 + dev->GetChannel()->GetDelay().ToDouble(Time::S));
      Ipv4Address dst = (m_node->GetId() == 0) ? Ipv4Address("10.1.1.2") : Ipv4Address("10.1.1.1");
      m_ipv4->GetRoutingTableEntry(dst).SetMetric(static_cast<uint16_t>(1.0 / distance));
    }
  }

private:
  Ptr<Node> m_node;
  Ptr<Ipv4> m_ipv4;
};

int main(int argc, char *argv[]) {
  LogComponentEnable("PoissonReverseSimulation", LOG_LEVEL_INFO);
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices = pointToPoint.Install(nodes.Get(0), nodes.Get(1));

  InternetStackHelper stack;
  stack.SetRoutingAlgorithm("ns3::StaticRoutingHelper");
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.252");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Install custom routing protocol
  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    Ptr<PoissonReverseRouting> prRouting = CreateObject<PoissonReverseRouting>();
    prRouting->SetNode(nodes.Get(i));
    nodes.Get(i)->GetObject<Ipv4>()->SetRoutingProtocol(prRouting, 0);
    Simulator::ScheduleRepeating(Seconds(5), &PoissonReverseRouting::UpdateRoutes, prRouting);
  }

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(5));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}