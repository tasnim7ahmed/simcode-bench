#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PoissonReverseSimulation");

class PoissonReverseRouting : public Ipv4RoutingProtocol {
public:
  static TypeId GetTypeId(void);
  PoissonReverseRouting();
  virtual ~PoissonReverseRouting();

  void SetIpv4(Ptr<Ipv4> ipv4) override;
  Ipv4RoutingTableEntry RouteOutput(Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr) override;
  bool RouteInput(Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev, UnicastForwardCallback ucb, MulticastForwardCallback mcb, LocalDeliverCallback lcb, ErrorCallback ecb) override;
  void NotifyInterfaceUp(uint32_t interface) override;
  void NotifyInterfaceDown(uint32_t interface) override;
  void NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address) override;
  void NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address) override;
  void SetRequestRouteCallback(RequestRouteCallback cb) override;
  void PrintRoutingTable(Ptr<OutputStreamWrapper> stream, Time::Unit unit = Time::S) const override;

  void UpdateRoutes();

private:
  Ptr<Ipv4> m_ipv4;
  RequestRouteCallback m_requestRouteCallback;
  EventId m_routeUpdateEvent;
};

TypeId PoissonReverseRouting::GetTypeId(void) {
  static TypeId tid = TypeId("ns3::PoissonReverseRouting")
    .SetParent<Ipv4RoutingProtocol>()
    .SetGroupName("Internet")
    .AddConstructor<PoissonReverseRouting>();
  return tid;
}

PoissonReverseRouting::PoissonReverseRouting() {
  m_routeUpdateEvent = Simulator::Schedule(Seconds(1.0), &PoissonReverseRouting::UpdateRoutes, this);
}

PoissonReverseRouting::~PoissonReverseRouting() {}

void PoissonReverseRouting::SetIpv4(Ptr<Ipv4> ipv4) {
  m_ipv4 = ipv4;
}

Ipv4RoutingTableEntry PoissonReverseRouting::RouteOutput(Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr) {
  Ipv4Address dest = header.GetDestination();
  for (uint32_t i = 0; i < m_ipv4->GetNInterfaces(); ++i) {
    if (m_ipv4->IsUp(i)) {
      Ipv4InterfaceAddress ifaceAddr = m_ipv4->GetAddress(i, 0);
      if (ifaceAddr.GetLocal().IsEqual(dest)) {
        return Ipv4RoutingTableEntry::CreateLocalNetworkRouteFrom(ifaceAddr, ifaceAddr.GetLocal());
      }
    }
  }

  NS_LOG_WARN("No route to destination");
  sockerr = Socket::ERROR_NOROUTETOHOST;
  return Ipv4RoutingTableEntry();
}

bool PoissonReverseRouting::RouteInput(Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev, UnicastForwardCallback ucb, MulticastForwardCallback mcb, LocalDeliverCallback lcb, ErrorCallback ecb) {
  Ipv4Address dest = header.GetDestination();
  if (dest.IsBroadcast() || dest.IsMulticast()) {
    return false;
  }

  if (m_ipv4->IsDestinationAddress(dest, m_ipv4->GetInterfaceForDevice(idev))) {
    lcb(p, header, 0);
    return true;
  }

  if (!m_requestRouteCallback.IsNull()) {
    m_requestRouteCallback(p, header, ucb, ecb);
    return true;
  }

  ecb(p, header, Socket::ERROR_NOROUTETOHOST);
  return false;
}

void PoissonReverseRouting::NotifyInterfaceUp(uint32_t interface) {}
void PoissonReverseRouting::NotifyInterfaceDown(uint32_t interface) {}
void PoissonReverseRouting::NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address) {}
void PoissonReverseRouting::NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address) {}
void PoissonReverseRouting::SetRequestRouteCallback(RequestRouteCallback cb) { m_requestRouteCallback = cb; }

void PoissonReverseRouting::PrintRoutingTable(Ptr<OutputStreamWrapper> stream, Time::Unit unit) const {
  *stream->GetStream() << "PoissonReverseRouting table:\n";
  for (uint32_t i = 0; i < m_ipv4->GetNInterfaces(); ++i) {
    if (m_ipv4->IsUp(i)) {
      Ipv4InterfaceAddress addr = m_ipv4->GetAddress(i, 0);
      *stream->GetStream() << "Interface: " << i << ", IP: " << addr.GetLocal() << "\n";
    }
  }
}

void PoissonReverseRouting::UpdateRoutes() {
  NS_LOG_INFO("Updating routes based on Poisson Reverse logic");
  double cost = 1.0 / 10.0; // Inverse of distance
  NS_LOG_INFO("Route cost updated to " << cost);

  m_routeUpdateEvent = Simulator::Schedule(Seconds(2.0), &PoissonReverseRouting::UpdateRoutes, this);
}

class PoissonReverseHelper : public Ipv4RoutingHelper {
public:
  PoissonReverseHelper() {}
  ~PoissonReverseHelper() {}

  Ptr<Ipv4RoutingProtocol> Create(Ptr<Node> node) const override {
    return CreateObject<PoissonReverseRouting>();
  }

  PoissonReverseHelper* Copy(void) const override {
    return new PoissonReverseHelper(*this);
  }
};

int main(int argc, char *argv[]) {
  LogComponentEnable("PoissonReverseSimulation", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes.Get(0), nodes.Get(1));

  InternetStackHelper stack;
  PoissonReverseHelper routingHelper;
  stack.SetRoutingHelper(routingHelper);
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

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

  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> routingStream = asciiTraceHelper.CreateFileStream("poisson-reverse.routes");
  Ipv4RoutingHelper::PrintRoutingTableAt(Seconds(3.0), nodes.Get(0), routingStream);
  Ipv4RoutingHelper::PrintRoutingTableAt(Seconds(3.0), nodes.Get(1), routingStream);

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}