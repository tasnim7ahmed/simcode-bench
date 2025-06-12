#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PoissonReverseSimulation");

class PoissonReverseRouting : public Ipv4RoutingProtocol {
public:
  static TypeId GetTypeId(void) {
    static TypeId tid = TypeId("PoissonReverseRouting")
                            .SetParent<Ipv4RoutingProtocol>()
                            .AddConstructor<PoissonReverseRouting>();
    return tid;
  }

  PoissonReverseRouting() : m_node(nullptr), m_interface(0) {}

  void SetNode(Ptr<Node> node) { m_node = node; }
  void SetInterface(uint32_t interface) { m_interface = interface; }

  virtual Ptr<Ipv4Route> RouteOutput(Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr) override {
    NS_LOG_FUNCTION(this << p << header.GetDestination() << oif);
    Ptr<Ipv4Route> route = Create<Ipv4Route>();
    route->SetDestination(header.GetDestination());
    route->SetGateway(Ipv4Address("1.0.0.2")); // Static for simplicity; in real case, update dynamically
    route->SetSource(m_node->GetObject<Ipv4>()->GetAddress(m_interface, 0).GetLocal());
    route->SetOutputDevice(m_node->GetDevice(m_interface));
    return route;
  }

  virtual bool RouteInput(Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
                          UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                          LocalDeliverCallback lcb, ErrorCallback ecb) override {
    NS_LOG_FUNCTION(this << p << header.GetDestination() << idev);
    if (header.GetDestination() == m_node->GetObject<Ipv4>()->GetAddress(m_interface, 0).GetLocal()) {
      lcb(p, header, 0);
      return true;
    }
    ecb(p, header, Socket::ERROR_NOROUTE);
    return false;
  }

  virtual void NotifyInterfaceUp(uint32_t interface) override {}
  virtual void NotifyInterfaceDown(uint32_t interface) override {}
  virtual void NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address) override {}
  virtual void NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address) override {}
  virtual void SetIpv4(Ptr<Ipv4> ipv4) override { m_ipv4 = ipv4; }
  virtual void PrintRoutingTable(Ptr<OutputStreamWrapper> stream) const override {}

private:
  Ptr<Node> m_node;
  uint32_t m_interface;
  Ptr<Ipv4> m_ipv4;
};

static void UpdateRoutes(Ptr<PoissonReverseRouting> routing, Time interval) {
  Simulator::Schedule(interval, &UpdateRoutes, routing, interval);

  // Simulate dynamic cost update based on inverse of distance
  double distance = 1.0 / (1.0 + Seconds(Simulator::Now().GetSeconds()).Get()); // dummy logic
  NS_LOG_INFO("Updating routes with inverse distance cost: " << distance);
}

int main(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("PoissonReverseSimulation", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices = pointToPoint.Install(nodes.Get(0), nodes.Get(1));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("1.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Install custom routing protocol
  Ptr<PoissonReverseRouting> routing0 = CreateObject<PoissonReverseRouting>();
  routing0->SetNode(nodes.Get(0));
  routing0->SetInterface(1); // assuming device index

  Ptr<PoissonReverseRouting> routing1 = CreateObject<PoissonReverseRouting>();
  routing1->SetNode(nodes.Get(1));
  routing1->SetInterface(1);

  Ipv4ListRoutingHelper listRouting;
  listRouting.Add(routing0, 1);
  listRouting.Add(routing1, 1);

  InternetStackHelper internet;
  InternetStackHelper internetNodes;
  internetNodes.SetRoutingHelper(listRouting);
  internetNodes.Install(nodes);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(5));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Schedule periodic route updates
  Simulator::Schedule(Seconds(1.0), &UpdateRoutes, routing0, Seconds(2.0));
  Simulator::Schedule(Seconds(1.0), &UpdateRoutes, routing1, Seconds(2.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}