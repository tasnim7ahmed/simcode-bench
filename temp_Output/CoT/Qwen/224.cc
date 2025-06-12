#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetAodvSimulation");

class UdpRandomNeighborApplication : public Application
{
public:
  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("UdpRandomNeighborApplication")
      .SetParent<Application> ()
      .AddConstructor<UdpRandomNeighborApplication> ()
      .AddAttribute ("Interval", "The interval between packet sends",
                     TimeValue (Seconds(1.0)),
                     MakeTimeAccessor (&UdpRandomNeighborApplication::m_interval),
                     MakeTimeChecker ())
      .AddAttribute ("PacketSize", "The size of packets sent in bytes",
                     UintegerValue(512),
                     MakeUintegerAccessor (&UdpRandomNeighborApplication::m_pktSize),
                     MakeUintegerChecker<uint32_t>());
    return tid;
  }

  UdpRandomNeighborApplication() {}
  virtual ~UdpRandomNeighborApplication() {}

protected:
  void DoInitialize() override
  {
    Application::DoInitialize();
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Bind();
  }

  void StartApplication() override
  {
    m_sendEvent = Simulator::Schedule(Seconds(0.0), &UdpRandomNeighborApplication::SendPacket, this);
  }

  void StopApplication() override
  {
    if (m_sendEvent.IsRunning())
      Simulator::Cancel(m_sendEvent);
  }

private:
  void SendPacket()
  {
    Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable>();
    uint32_t numNodes = NodeList::GetNNodes();
    uint32_t destIndex = rng->GetInteger(0, numNodes - 1);
    while (destIndex == GetNode()->GetId())
      {
        destIndex = rng->GetInteger(0, numNodes - 1);
      }

    Ptr<Node> destNode = NodeList::GetNode(destIndex);
    Ipv4Address destAddr = destNode->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();

    Ptr<Packet> packet = Create<Packet>(m_pktSize);
    m_socket->SendTo(packet, 0, InetSocketAddress(destAddr, 9));
    m_sendEvent = Simulator::Schedule(m_interval, &UdpRandomNeighborApplication::SendPacket, this);
  }

  Time m_interval;
  uint32_t m_pktSize;
  EventId m_sendEvent;
  Ptr<Socket> m_socket;
};

int main(int argc, char *argv[])
{
  uint32_t numNodes = 10;
  double radius = 100.0;
  double duration = 20.0;

  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));

  NodeContainer nodes;
  nodes.Create(numNodes);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
  mac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(wifiPhy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                "X", StringValue("100.0"),
                                "Y", StringValue("100.0"),
                                "Rho", StringValue("ns3::UniformRandomVariable[Min=0|Max=" + std::to_string(radius) + "]"));
  mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                            "Speed", StringValue("ns3::ConstantVariable[Constant=5]"),
                            "Pause", StringValue("ns3::ConstantVariable[Constant=2]"),
                            "PositionAllocator", PointerValue(mobility.GetPositionAllocator()));
  mobility.Install(nodes);

  AodvHelper aodv;
  Ipv4ListRoutingHelper listRouting;
  listRouting.Add(aodv, 10);

  InternetStackHelper internet;
  internet.SetRoutingHelper(listRouting);
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  for (uint32_t i = 0; i < nodes.GetN(); ++i)
  {
    Ptr<UdpRandomNeighborApplication> app = CreateObject<UdpRandomNeighborApplication>();
    nodes.Get(i)->AddApplication(app);
    app->SetStartTime(Seconds(0.0));
    app->SetStopTime(Seconds(duration));
  }

  Simulator::Stop(Seconds(duration));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}