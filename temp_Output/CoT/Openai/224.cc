#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include <random>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetAodvUdpRandomNeighbor");

class PeriodicNeighborSender : public Application
{
public:
  PeriodicNeighborSender();
  virtual ~PeriodicNeighborSender();
  void Setup(Ptr<Node> node, Ipv4InterfaceContainer interfaces, uint16_t port, Time period, Ptr<UniformRandomVariable> uv);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);
  void ScheduleNextTx();
  void SendPacketToRandomNeighbor();

  Ptr<Node> m_node;
  Ipv4InterfaceContainer m_interfaces;
  uint16_t m_port;
  Time m_period;
  EventId m_sendEvent;
  Ptr<UniformRandomVariable> m_uv;
  Ptr<Socket> m_socket;
};

PeriodicNeighborSender::PeriodicNeighborSender()
  : m_node(nullptr), m_port(0), m_period(Seconds(1.0)), m_sendEvent(), m_socket(nullptr)
{
}

PeriodicNeighborSender::~PeriodicNeighborSender()
{
  m_socket = nullptr;
}

void
PeriodicNeighborSender::Setup(Ptr<Node> node, Ipv4InterfaceContainer interfaces, uint16_t port, Time period, Ptr<UniformRandomVariable> uv)
{
  m_node = node;
  m_interfaces = interfaces;
  m_port = port;
  m_period = period;
  m_uv = uv;
}

void
PeriodicNeighborSender::StartApplication()
{
  m_socket = Socket::CreateSocket(m_node, UdpSocketFactory::GetTypeId());
  ScheduleNextTx();
}

void
PeriodicNeighborSender::StopApplication()
{
  if (m_sendEvent.IsRunning())
    Simulator::Cancel(m_sendEvent);
  if (m_socket)
    m_socket->Close();
}

void
PeriodicNeighborSender::ScheduleNextTx()
{
  m_sendEvent = Simulator::Schedule(m_period, &PeriodicNeighborSender::SendPacketToRandomNeighbor, this);
}

void
PeriodicNeighborSender::SendPacketToRandomNeighbor()
{
  Ptr<Ipv4> ipv4 = m_node->GetObject<Ipv4>();
  Ipv4Address myAddress = ipv4->GetAddress(1, 0).GetLocal();
  std::vector<uint32_t> neighborIndices;
  for (uint32_t i=0; i<m_interfaces.GetN(); ++i)
  {
    if (m_interfaces.GetAddress(i) != myAddress)
      neighborIndices.push_back(i);
  }
  if (neighborIndices.size() > 0)
  {
    uint32_t idx = neighborIndices[std::min((uint32_t)m_uv->GetValue(0, neighborIndices.size()), (uint32_t)neighborIndices.size()-1)];
    Ipv4Address dst = m_interfaces.GetAddress(idx);
    Ptr<Packet> pkt = Create<Packet>(100);
    m_socket->SendTo(pkt, 0, InetSocketAddress(dst, m_port));
  }
  ScheduleNextTx();
}

int main(int argc, char *argv[])
{
  uint32_t numNodes = 10;
  double simTime = 20.0;
  double periodicInterval = 1.0;
  double txPower = 16.0206; // ~40m transmission range in Friis with default

  CommandLine cmd;
  cmd.AddValue("numNodes", "Number of MANET nodes.", numNodes);
  cmd.AddValue("simTime", "Simulation duration (seconds).", simTime);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(numNodes);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel(wifiChannel.Create());
  wifiPhy.Set("TxPowerStart", DoubleValue(txPower));
  wifiPhy.Set("TxPowerEnd", DoubleValue(txPower));

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                            "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=3.0]"),
                            "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
                            "PositionAllocator", PointerValue(CreateObjectWithAttributes<RandomRectanglePositionAllocator>(
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"))));
  mobility.Install(nodes);

  InternetStackHelper stack;
  AodvHelper aodv;
  stack.SetRoutingHelper(aodv);
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.0.0", "255.255.0.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t echoPort = 50000;
  ApplicationContainer sinkApps;
  for (uint32_t i = 0; i < nodes.GetN(); ++i)
  {
    UdpServerHelper echoServer(echoPort);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(i));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simTime));
    sinkApps.Add(serverApp);
  }

  Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
  for (uint32_t i = 0; i < nodes.GetN(); ++i)
  {
    Ptr<PeriodicNeighborSender> app = CreateObject<PeriodicNeighborSender>();
    app->Setup(nodes.Get(i), interfaces, echoPort, Seconds(periodicInterval), uv);
    nodes.Get(i)->AddApplication(app);
    app->SetStartTime(Seconds(0.5 + 0.1 * i));
    app->SetStopTime(Seconds(simTime));
  }

  wifiPhy.EnablePcap("manet-aodv-udp", devices);

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}