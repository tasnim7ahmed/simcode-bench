#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetUdpAodvSimulation");

class UdpNeighborTraffic : public Application
{
public:
  static TypeId GetTypeId();
  UdpNeighborTraffic();
  virtual ~UdpNeighborTraffic();

protected:
  void StartApplication() override;
  void StopApplication() override;

private:
  void SendPacket();
  void ScheduleTx();

  Ptr<Socket> m_socket;
  Address m_peerAddress;
  uint32_t m_packetSize;
  DataRate m_dataRate;
  EventId m_sendEvent;
  bool m_isRunning;
};

TypeId
UdpNeighborTraffic::GetTypeId()
{
  static TypeId tid = TypeId("UdpNeighborTraffic")
                          .SetParent<Application>()
                          .AddConstructor<UdpNeighborTraffic>();
  return tid;
}

UdpNeighborTraffic::UdpNeighborTraffic()
    : m_socket(nullptr),
      m_peerAddress(),
      m_packetSize(1024),
      m_dataRate("100kbps"),
      m_isRunning(false)
{
}

UdpNeighborTraffic::~UdpNeighborTraffic()
{
  m_socket = nullptr;
}

void
UdpNeighborTraffic::StartApplication()
{
  m_isRunning = true;
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
  m_socket->Bind(local);

  ScheduleTx();
}

void
UdpNeighborTraffic::StopApplication()
{
  m_isRunning = false;
  if (m_sendEvent.IsRunning())
  {
    Simulator::Cancel(m_sendEvent);
  }
  if (m_socket)
  {
    m_socket->Close();
  }
}

void
UdpNeighborTraffic::ScheduleTx()
{
  if (!m_isRunning)
    return;

  Time tNext(Seconds(1.0));
  m_sendEvent = Simulator::Schedule(tNext, &UdpNeighborTraffic::SendPacket, this);
}

void
UdpNeighborTraffic::SendPacket()
{
  if (!m_isRunning || m_peerAddress == Address())
  {
    ScheduleTx();
    return;
  }

  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->SendTo(packet, 0, m_peerAddress);
  ScheduleTx();
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  LogComponentEnable("UdpNeighborTraffic", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(10);

  WifiMacHelper wifiMac;
  WifiPhyHelper wifiPhy;
  wifiMac.SetType("ns3::AdhocWifiMac");
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(channel.Create());
  WifiHelper wifi;
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"));
  mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                            "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                            "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"));
  mobility.Install(nodes);

  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingHelper(aodv);
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  for (uint32_t i = 0; i < nodes.GetN(); ++i)
  {
    Ptr<Node> node = nodes.Get(i);
    Ptr<UdpNeighborTraffic> app = CreateObject<UdpNeighborTraffic>();
    node->AddApplication(app);
    app->SetStartTime(Seconds(0.0));
    app->SetStopTime(Seconds(20.0));
  }

  Config::Connect("/NodeList/*/ApplicationList/*/$UdpNeighborTraffic/BytesSent",
                  MakeCallback([](std::string path, Ptr<const Packet> packet) {
                    NS_LOG_INFO("Packet sent: " << packet->GetSize() << " bytes");
                  }));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(20.0));
  Simulator::Run();

  monitor->CheckForLostPackets();
  flowmon.SerializeToXmlFile("manet-aodv-udp.xml", false, false);

  Simulator::Destroy();
  return 0;
}