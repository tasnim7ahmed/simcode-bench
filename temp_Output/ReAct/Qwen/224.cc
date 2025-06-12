#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetSimulation");

class UdpNeighborSender : public Application
{
public:
  static TypeId GetTypeId();
  UdpNeighborSender();
  virtual ~UdpNeighborSender();

protected:
  void StartApplication() override;
  void StopApplication() override;

private:
  void SendPacket();
  void ScheduleNextPacket();
  uint32_t SelectRandomNeighbor();

  Ptr<Socket> m_socket;
  Address m_peerAddress;
  uint16_t m_peerPort;
  Time m_interval;
  uint32_t m_packetSize;
  EventId m_sendEvent;
  bool m_isStarted;
};

TypeId
UdpNeighborSender::GetTypeId()
{
  static TypeId tid = TypeId("UdpNeighborSender")
                          .SetParent<Application>()
                          .AddConstructor<UdpNeighborSender>();
  return tid;
}

UdpNeighborSender::UdpNeighborSender()
    : m_peerPort(9),
      m_interval(Seconds(1.0)),
      m_packetSize(512),
      m_isStarted(false)
{
}

UdpNeighborSender::~UdpNeighborSender()
{
  m_socket = nullptr;
}

void UdpNeighborSender::StartApplication()
{
  m_isStarted = true;
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_peerPort);
  m_socket->Bind(local);

  ScheduleNextPacket();
}

void UdpNeighborSender::StopApplication()
{
  m_isStarted = false;
  if (m_sendEvent.IsRunning())
  {
    Simulator::Cancel(m_sendEvent);
  }
  if (m_socket)
  {
    m_socket->Close();
  }
}

void UdpNeighborSender::ScheduleNextPacket()
{
  if (!m_isStarted)
    return;

  m_sendEvent = Simulator::Schedule(m_interval, &UdpNeighborSender::SendPacket, this);
}

void UdpNeighborSender::SendPacket()
{
  Ptr<UniformRandomVariable> urv = CreateObject<UniformRandomVariable>();
  uint32_t neighborIndex = SelectRandomNeighbor();
  if (neighborIndex == GetNode()->GetId())
    return; // no neighbors found

  Ptr<Node> peerNode = NodeList::GetNode(neighborIndex);
  if (!peerNode)
    return;

  Ptr<Ipv4> ipv4 = peerNode->GetObject<Ipv4>();
  Ipv4InterfaceAddress iaddr = ipv4->GetInterface(1)->GetAddress(0); // Assume wifi interface is index 1
  Ipv4Address peerIp = iaddr.GetLocal();

  NS_LOG_INFO("Node " << GetNode()->GetId() << " sending packet to Node " << peerNode->GetId());

  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->SendTo(packet, 0, InetSocketAddress(peerIp, m_peerPort));
  ScheduleNextPacket();
}

uint32_t
UdpNeighborSender::SelectRandomNeighbor()
{
  std::vector<uint32_t> neighbors;
  for (uint32_t i = 0; i < NodeList::GetNNodes(); ++i)
  {
    if (i != GetNode()->GetId())
    {
      neighbors.push_back(i);
    }
  }

  if (neighbors.empty())
    return GetNode()->GetId(); // invalid

  Ptr<UniformRandomVariable> urv = CreateObject<UniformRandomVariable>();
  return neighbors[urv->GetInteger(0, neighbors.size() - 1)];
}

int main(int argc, char *argv[])
{
  uint32_t numNodes = 10;
  double radius = 100.0;
  CommandLine cmd(__FILE__);
  cmd.AddValue("numNodes", "Number of nodes in the MANET", numNodes);
  cmd.AddValue("radius", "Radius of simulation area", radius);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::AodvRoutingProtocol::EnableHello", BooleanValue(true));
  Config::SetDefault("ns3::AodvRoutingProtocol::HelloInterval", TimeValue(Seconds(1.0)));
  Config::SetDefault("ns3::AodvRoutingProtocol::ActiveRouteTimeout", TimeValue(Seconds(3.0)));

  NodeContainer nodes;
  nodes.Create(numNodes);

  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiHelper wifi;
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
  mac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices;
  devices = wifi.Install(wifiPhy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(radius) + "]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(radius) + "]"));
  mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                            "Speed", StringValue("ns3::ConstantVariable[Constant=5.0]"),
                            "Pause", StringValue("ns3::ConstantVariable[Constant=2.0]"));
  mobility.Install(nodes);

  InternetStackHelper stack;
  AodvHelper aodv;
  stack.SetRoutingHelper(aodv);
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  for (uint32_t i = 0; i < nodes.GetN(); ++i)
  {
    Ptr<Node> node = nodes.Get(i);
    Ptr<UdpNeighborSender> app = CreateObject<UdpNeighborSender>();
    app->SetAttribute("Interval", TimeValue(Seconds(1.0)));
    app->SetAttribute("PacketSize", UintegerValue(512));
    node->AddApplication(app);
    app->SetStartTime(Seconds(0.0));
    app->SetStopTime(Seconds(20.0));
  }

  Simulator::Stop(Seconds(20.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}