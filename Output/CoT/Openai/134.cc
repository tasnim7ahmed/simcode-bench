#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PoissonReverseSimulation");

class PoissonReverseRouting : public Object
{
public:
  PoissonReverseRouting (Ptr<Node> node, Ipv4Address ownAddress, Ipv4Address peerAddress, Ptr<NetDevice> dev, double linkDistance);
  void Start ();
  double GetRouteCost () const;
  void LogRouteUpdate ();
  void InstallRecvCallback ();
private:
  void ScheduleUpdate ();
  void UpdateRoute ();
  void ReceivePacket (Ptr<Socket> socket);
  Ptr<Node> m_node;
  Ipv4Address m_ownAddress;
  Ipv4Address m_peerAddress;
  Ptr<NetDevice> m_dev;
  double m_linkDistance;
  double m_cost;
  EventId m_event;
  Ptr<UniformRandomVariable> m_rand;
  Ptr<Socket> m_socket;
  static constexpr double kUpdateInterval = 1.0; // seconds
};

PoissonReverseRouting::PoissonReverseRouting (Ptr<Node> node, Ipv4Address ownAddress, Ipv4Address peerAddress, Ptr<NetDevice> dev, double linkDistance)
  : m_node(node),
    m_ownAddress(ownAddress),
    m_peerAddress(peerAddress),
    m_dev(dev),
    m_linkDistance(linkDistance)
{
  m_cost = 1.0 / m_linkDistance;
  m_rand = CreateObject<UniformRandomVariable>();
}

double
PoissonReverseRouting::GetRouteCost () const
{
  return m_cost;
}

void
PoissonReverseRouting::LogRouteUpdate ()
{
  NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "s "
      << "Node " << m_node->GetId ()
      << " updated route cost to " << m_peerAddress
      << ": " << m_cost);
}

void
PoissonReverseRouting::Start ()
{
  InstallRecvCallback ();
  ScheduleUpdate ();
}

void
PoissonReverseRouting::ScheduleUpdate ()
{
  m_event = Simulator::Schedule (Seconds(kUpdateInterval), &PoissonReverseRouting::UpdateRoute, this);
}

void
PoissonReverseRouting::UpdateRoute ()
{
  double noise = m_rand->GetValue (-0.2, 0.2);
  m_cost = 1.0 / m_linkDistance + noise;
  if (m_cost < 0.1)
    m_cost = 0.1;

  LogRouteUpdate ();

  // Send a dummy "routing update" packet to peer (in real RP: carry cost info)
  Ptr<Socket> sendSock = Socket::CreateSocket(m_node, TypeId::LookupByName("ns3::UdpSocketFactory"));
  InetSocketAddress remote = InetSocketAddress(m_peerAddress, 9999);
  sendSock->Connect(remote);
  Ptr<Packet> pkt = Create<Packet>((uint8_t*)&m_cost, sizeof(m_cost));
  sendSock->Send(pkt);
  sendSock->Close();

  ScheduleUpdate ();
}

void
PoissonReverseRouting::InstallRecvCallback ()
{
  m_socket = Socket::CreateSocket(m_node, TypeId::LookupByName("ns3::UdpSocketFactory"));
  InetSocketAddress local = InetSocketAddress(m_ownAddress, 9999);
  m_socket->Bind(local);
  m_socket->SetRecvCallback(MakeCallback(&PoissonReverseRouting::ReceivePacket, this));
}

void
PoissonReverseRouting::ReceivePacket (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet = socket->RecvFrom(from);
  double recvCost = 0;
  packet->CopyData((uint8_t*)&recvCost, sizeof(recvCost));
  NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "s "
      << "Node " << m_node->GetId ()
      << " received routing update from " << InetSocketAddress::ConvertFrom(from).GetIpv4 ()
      << " with cost " << recvCost);
}

int
main (int argc, char *argv[])
{
  LogComponentEnable("PoissonReverseSimulation", LOG_LEVEL_INFO);

  double distance = 10.0; // meters (for cost calculation)
  uint32_t packetSize = 1024;
  double simTime = 10.0;

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices = p2p.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  Ptr<PoissonReverseRouting> pr1 = CreateObject<PoissonReverseRouting>(
    nodes.Get(0), interfaces.GetAddress(0), interfaces.GetAddress(1), devices.Get(0), distance);
  Ptr<PoissonReverseRouting> pr2 = CreateObject<PoissonReverseRouting>(
    nodes.Get(1), interfaces.GetAddress(1), interfaces.GetAddress(0), devices.Get(1), distance);

  pr1->Start();
  pr2->Start();

  // Set up CBR UDP traffic from Node 0 to Node 1
  uint16_t port = 5000;
  OnOffHelper onoff ("ns3::UdpSocketFactory",
                     Address (InetSocketAddress (interfaces.GetAddress (1), port)));
  onoff.SetAttribute ("DataRate", StringValue ("1Mbps"));
  onoff.SetAttribute ("PacketSize", UintegerValue (packetSize));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (2.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (simTime)));

  ApplicationContainer apps = onoff.Install (nodes.Get (0));

  PacketSinkHelper sink ("ns3::UdpSocketFactory",
                        Address (InetSocketAddress (Ipv4Address::GetAny (), port)));
  apps.Add (sink.Install (nodes.Get (1)));

  Simulator::Stop(Seconds(simTime));

  Simulator::Run();

  // Statistics output
  Ptr<PacketSink> ps = DynamicCast<PacketSink> (apps.Get (1));
  if (ps)
  {
    std::cout << "Total Bytes Received: " << ps->GetTotalRx () << std::endl;
  }

  Simulator::Destroy();
  return 0;
}