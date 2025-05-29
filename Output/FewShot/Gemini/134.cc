#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"
#include "ns3/netanim-module.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/vector.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PoissonReverseSimulation");

class PoissonReverseRoutingProtocol : public Ipv4RoutingProtocol
{
public:
  static TypeId GetTypeId();
  PoissonReverseRoutingProtocol();
  virtual ~PoissonReverseRoutingProtocol();

  void SetUpdateInterval(Time interval);
  void SetHelloInterval(Time interval);
  void SetMaxCost(double maxCost);

private:
  virtual void Start();
  virtual void Stop();
  virtual Ptr<Ipv4Route> RouteOutput(Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &errno);
  virtual bool RouteInput(Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev, UnicastForwardCallback ucb, MulticastForwardCallback mcb, LocalDeliverCallback lcb, ErrorCallback ecb);

  void UpdateRoutes();
  void SendHello();
  void ReceiveHello(Ptr<Packet> p, const Address &src, const Address &dst);

  Time m_updateInterval;
  Time m_helloInterval;
  double m_maxCost;
  Ipv4InterfaceAddress m_interface;
  EventId m_updateEvent;
  EventId m_helloEvent;
  std::map<Ipv4Address, double> m_routeCosts;
  std::map<Ipv4Address, Ipv4Address> m_nextHop;
  std::map<Ipv4Address, Ptr<NetDevice>> m_outputDevice;
  uint16_t m_helloPort;
  Ptr<Socket> m_helloSocket;
};

NS_OBJECT_ENSURE_REGISTERED(PoissonReverseRoutingProtocol);

TypeId
PoissonReverseRoutingProtocol::GetTypeId()
{
  static TypeId tid = TypeId("PoissonReverseRoutingProtocol")
                            .SetParent<Ipv4RoutingProtocol>()
                            .SetGroupName("Internet")
                            .AddConstructor<PoissonReverseRoutingProtocol>()
                            .AddAttribute("UpdateInterval",
                                          "The time between route updates",
                                          TimeValue(Seconds(5.0)),
                                          MakeTimeAccessor(&PoissonReverseRoutingProtocol::m_updateInterval),
                                          MakeTimeChecker())
                            .AddAttribute("HelloInterval",
                                          "The time between hello messages",
                                          TimeValue(Seconds(1.0)),
                                          MakeTimeAccessor(&PoissonReverseRoutingProtocol::m_helloInterval),
                                          MakeTimeChecker())
                            .AddAttribute("MaxCost",
                                          "The maximum cost for a route",
                                          DoubleValue(1000.0),
                                          MakeDoubleAccessor(&PoissonReverseRoutingProtocol::m_maxCost),
                                          MakeDoubleChecker<double>());
  return tid;
}

PoissonReverseRoutingProtocol::PoissonReverseRoutingProtocol()
    : m_updateInterval(Seconds(5.0)),
      m_helloInterval(Seconds(1.0)),
      m_maxCost(1000.0),
      m_helloPort(50000)
{
  NS_LOG_FUNCTION(this);
}

PoissonReverseRoutingProtocol::~PoissonReverseRoutingProtocol()
{
  NS_LOG_FUNCTION(this);
}

void
PoissonReverseRoutingProtocol::SetUpdateInterval(Time interval)
{
  NS_LOG_FUNCTION(this << interval);
  m_updateInterval = interval;
}

void
PoissonReverseRoutingProtocol::SetHelloInterval(Time interval)
{
  NS_LOG_FUNCTION(this << interval);
  m_helloInterval = interval;
}

void
PoissonReverseRoutingProtocol::SetMaxCost(double maxCost)
{
  NS_LOG_FUNCTION(this << maxCost);
  m_maxCost = maxCost;
}

void
PoissonReverseRoutingProtocol::Start()
{
  NS_LOG_FUNCTION(this);
  if (m_ipv4 == 0)
  {
    NS_FATAL_ERROR("PoissonReverseRoutingProtocol::Start(): IPv4 interface is null");
  }

  m_interface = m_ipv4->GetAddress(0, 0);

  m_updateEvent = Simulator::Schedule(Seconds(0.1), &PoissonReverseRoutingProtocol::UpdateRoutes, this);
  m_helloEvent = Simulator::Schedule(Seconds(0.2), &PoissonReverseRoutingProtocol::SendHello, this);

  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  m_helloSocket = Socket::CreateSocket(GetObject<Node>(), tid);
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_helloPort);
  m_helloSocket->Bind(local);
  m_helloSocket->SetRecvCallback(MakeCallback(&PoissonReverseRoutingProtocol::ReceiveHello, this));
  m_helloSocket->SetAllowBroadcast(true);
}

void
PoissonReverseRoutingProtocol::Stop()
{
  NS_LOG_FUNCTION(this);
  Simulator::Cancel(m_updateEvent);
  Simulator::Cancel(m_helloEvent);
  m_helloSocket->Close();
}

Ptr<Ipv4Route>
PoissonReverseRoutingProtocol::RouteOutput(Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &errno)
{
  NS_LOG_FUNCTION(this << p << header << oif << errno);

  Ipv4Address dst = header.GetDestination();

  if (m_nextHop.find(dst) != m_nextHop.end())
  {
    Ptr<Ipv4Route> route = Create<Ipv4Route>();
    route->SetOutputTtl(header.GetTtl());
    route->SetDestination(dst);
    route->SetGateway(m_nextHop[dst]);
    route->SetOutputDevice(m_outputDevice[dst]);
    errno = Socket::ERROR_NOTERROR;
    return route;
  }

  errno = Socket::ERROR_NOROUTETOHOST;
  return 0;
}

bool
PoissonReverseRoutingProtocol::RouteInput(Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev, UnicastForwardCallback ucb, MulticastForwardCallback mcb, LocalDeliverCallback lcb, ErrorCallback ecb)
{
  NS_LOG_FUNCTION(this << p << header << idev);
  Ipv4Address dst = header.GetDestination();

  if (m_interface.GetAddress() == dst)
  {
    NS_LOG_LOGIC("Forwarding packet to local");
    lcb(p, header, idev);
    return true;
  }
  else
  {
    NS_LOG_LOGIC("UnicastForwardCallback");
    ucb(p, header, idev);
    return true;
  }
}

void
PoissonReverseRoutingProtocol::UpdateRoutes()
{
  NS_LOG_FUNCTION(this);
  NS_LOG_INFO("Updating routes at time " << Simulator::Now().Seconds());

  for (auto const& [dest, cost] : m_routeCosts) {
    NS_LOG_INFO("Route to " << dest << " cost: " << cost);
  }

  m_updateEvent = Simulator::Schedule(m_updateInterval, &PoissonReverseRoutingProtocol::UpdateRoutes, this);
}

void
PoissonReverseRoutingProtocol::SendHello()
{
  NS_LOG_FUNCTION(this);

  Ptr<Packet> packet = Create<Packet>();
  std::ostringstream os;
  os << "Hello from " << m_interface.GetAddress();
  packet->AddAtEnd(Create<Packet>(reinterpret_cast<const uint8_t*>(os.str().c_str()), os.str().length()));

  InetSocketAddress dst = InetSocketAddress(Ipv4Address::GetBroadcast(), m_helloPort);
  m_helloSocket->SendTo(packet, 0, dst);

  m_helloEvent = Simulator::Schedule(m_helloInterval, &PoissonReverseRoutingProtocol::SendHello, this);
}

void
PoissonReverseRoutingProtocol::ReceiveHello(Ptr<Packet> p, const Address &src, const Address &dst)
{
  NS_LOG_FUNCTION(this << p << src << dst);

  InetSocketAddress sourceAddr = InetSocketAddress::ConvertFrom(src);
  Ipv4Address sourceIp = sourceAddr.GetIpv4();

  Ptr<NetDevice> inputDevice = 0;
  for (uint32_t i = 0; i < m_ipv4->GetNInterfaces(); ++i) {
        if (m_ipv4->GetAddress(i, 0).GetLocal() == m_interface.GetAddress()) {
            inputDevice = m_ipv4->GetNetDevice(i);
            break;
        }
  }

  if (!inputDevice) {
      NS_LOG_ERROR("No matching interface found");
      return;
  }

  double distance = 1.0; // Base cost, can be adjusted based on link characteristics

  m_routeCosts[sourceIp] = distance;
  m_nextHop[sourceIp] = sourceIp;
  m_outputDevice[sourceIp] = inputDevice;

  NS_LOG_INFO("Received Hello from " << sourceIp << " cost set to " << distance);
}

int main(int argc, char *argv[])
{
  LogComponentEnable("PoissonReverseSimulation", LOG_LEVEL_INFO);

  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.SetRoutingProtocol("ns3::PoissonReverseRoutingProtocol");
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  Ptr<PoissonReverseRoutingProtocol> proto1 = DynamicCast<PoissonReverseRoutingProtocol>(nodes.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol());
  Ptr<PoissonReverseRoutingProtocol> proto2 = DynamicCast<PoissonReverseRoutingProtocol>(nodes.Get(1)->GetObject<Ipv4>()->GetRoutingProtocol());

  uint16_t port = 9;
  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApp = echoServer.Install(nodes.Get(1));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(5));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));


  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}