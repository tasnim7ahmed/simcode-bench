#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/energy-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessSensorNetworkEnergySimulation");

class SensorNodeApplication : public Application
{
public:
  static TypeId GetTypeId();
  SensorNodeApplication();
  virtual ~SensorNodeApplication();

protected:
  virtual void StartApplication();
  virtual void StopApplication();

private:
  void SendPacket();
  uint32_t m_packetSize;
  Time m_interval;
  Ptr<Socket> m_socket;
  Address m_sinkAddress;
  bool m_isSink;
};

TypeId
SensorNodeApplication::GetTypeId()
{
  static TypeId tid = TypeId("SensorNodeApplication")
                          .SetParent<Application>()
                          .AddConstructor<SensorNodeApplication>()
                          .AddAttribute("Interval", "Time between packet sends",
                                        TimeValue(Seconds(1)),
                                        MakeTimeAccessor(&SensorNodeApplication::m_interval),
                                        MakeTimeChecker())
                          .AddAttribute("PacketSize", "Size of packets sent in bytes",
                                        UintegerValue(50),
                                        MakeUintegerAccessor(&SensorNodeApplication::m_packetSize),
                                        MakeUintegerChecker<uint32_t>());
  return tid;
}

SensorNodeApplication::SensorNodeApplication()
    : m_packetSize(50), m_interval(Seconds(1)), m_socket(nullptr), m_isSink(false)
{
}

SensorNodeApplication::~SensorNodeApplication()
{
  m_socket = nullptr;
}

void SensorNodeApplication::StartApplication()
{
  if (m_isSink)
    return;

  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());

  InetSocketAddress sinkAddr = InetSocketAddress(Ipv4Address("10.1.1.1"), 9);
  m_sinkAddress = sinkAddr;

  Simulator::ScheduleNow(&SensorNodeApplication::SendPacket, this);
}

void SensorNodeApplication::StopApplication()
{
  if (m_socket)
    {
      m_socket->Close();
      m_socket = nullptr;
    }
}

void SensorNodeApplication::SendPacket()
{
  if (m_isSink)
    return;

  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->SendTo(packet, 0, m_sinkAddress);

  NS_LOG_DEBUG("Sent packet of size " << m_packetSize << " to sink at time " << Now().As(Time::S));

  Simulator::Schedule(m_interval, &SensorNodeApplication::SendPacket, this);
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  LogComponentEnable("SensorNodeApplication", LOG_LEVEL_DEBUG);

  double simulationTime = 1000.0;
  uint32_t gridSize = 5;
  uint32_t totalNodes = gridSize * gridSize;
  uint32_t sinkNodeId = 0;

  NodeContainer nodes;
  nodes.Create(totalNodes + 1); // +1 for the sink node

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("OfdmRate6Mbps"),
                               "ControlMode", StringValue("OfdmRate6Mbps"));

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(10.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(gridSize),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Mark sink node
  Ptr<Node> sinkNode = nodes.Get(sinkNodeId);
  Ptr<SensorNodeApplication> sinkApp = CreateObject<SensorNodeApplication>();
  sinkApp->m_isSink = true;
  sinkNode->AddApplication(sinkApp);
  sinkApp->SetStartTime(Seconds(0));
  sinkApp->SetStopTime(Seconds(simulationTime));

  // Install sensor applications on all other nodes
  for (uint32_t i = 1; i < nodes.GetN(); ++i)
    {
      Ptr<Node> node = nodes.Get(i);
      if (node == sinkNode)
        continue;

      Ptr<SensorNodeApplication> app = CreateObject<SensorNodeApplication>();
      app->SetAttribute("Interval", TimeValue(Seconds(1)));
      app->SetAttribute("PacketSize", UintegerValue(50));
      node->AddApplication(app);
      app->SetStartTime(Seconds(0));
      app->SetStopTime(Seconds(simulationTime));
    }

  // Energy model setup
  BasicEnergySourceHelper energySourceHelper;
  energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(100.0)); // Each node starts with 100 Joules

  WifiRadioEnergyModelHelper radioEnergyHelper;
  radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.350)); // Transmission current
  radioEnergyHelper.Set("RxCurrentA", DoubleValue(0.190)); // Reception current

  EnergyModelContainer energyModels = radioEnergyHelper.Install(devices, energySourceHelper.Install(nodes));

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}