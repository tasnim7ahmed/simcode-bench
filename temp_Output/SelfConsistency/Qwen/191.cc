#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/energy-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessSensorNetworkEnergy");

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
  EventId m_sendEvent;
};

TypeId
SensorNodeApplication::GetTypeId()
{
  static TypeId tid = TypeId("SensorNodeApplication")
                          .SetParent<Application>()
                          .AddConstructor<SensorNodeApplication>()
                          .AddAttribute("PacketSize", "Size of packets sent.",
                                        UintegerValue(100),
                                        MakeUintegerAccessor(&SensorNodeApplication::m_packetSize),
                                        MakeUintegerChecker<uint32_t>())
                          .AddAttribute("Interval", "Interval between packets.",
                                        TimeValue(Seconds(1)),
                                        MakeTimeAccessor(&SensorNodeApplication::m_interval),
                                        MakeTimeChecker());
  return tid;
}

SensorNodeApplication::SensorNodeApplication()
    : m_packetSize(100), m_interval(Seconds(1)), m_socket(nullptr)
{
}

SensorNodeApplication::~SensorNodeApplication()
{
  m_socket = nullptr;
}

void
SensorNodeApplication::StartApplication()
{
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  InetSocketAddress sinkAddr = InetSocketAddress(Ipv4Address("10.0.0.1"), 9);
  m_sinkAddress = sinkAddr;
  SendPacket();
}

void
SensorNodeApplication::StopApplication()
{
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
SensorNodeApplication::SendPacket()
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->SendTo(packet, 0, m_sinkAddress);

  // Schedule next packet only if the node has energy
  Ptr<EnergySource> energySource = GetNode()->GetObject<EnergySourceContainer>()->Get(0);
  if (energySource && energySource->GetRemainingEnergy() > 0)
  {
    m_sendEvent = Simulator::Schedule(m_interval, &SensorNodeApplication::SendPacket, this);
  }
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  uint32_t numNodesPerSide = 5; // Grid of 5x5 nodes
  double gridStep = 50.0;
  double totalTime = 1000.0;

  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(100));

  NodeContainer sensorNodes;
  sensorNodes.Create(numNodesPerSide * numNodesPerSide);

  NodeContainer sinkNode;
  sinkNode.Create(1);

  // Mobility setup
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(gridStep),
                                "DeltaY", DoubleValue(gridStep),
                                "GridWidth", UintegerValue(numNodesPerSide),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(sensorNodes);
  mobility.Install(sinkNode);

  // Set position of sink node at center
  Ptr<Node> sink = sinkNode.Get(0);
  Ptr<MobilityModel> sinkMobility = sink->GetObject<MobilityModel>();
  sinkMobility->SetPosition(Vector((numNodesPerSide / 2) * gridStep,
                                  (numNodesPerSide / 2) * gridStep, 0.0));

  // Wifi setup
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
  wifiMac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer sensorDevices = wifi.Install(wifiPhy, wifiMac, sensorNodes);
  NetDeviceContainer sinkDevice = wifi.Install(wifiPhy, wifiMac, sinkNode);

  // Internet stack
  InternetStackHelper stack;
  stack.Install(sensorNodes);
  stack.Install(sinkNode);

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");

  Ipv4InterfaceContainer sensorInterfaces = address.Assign(sensorDevices);
  Ipv4InterfaceContainer sinkInterface = address.Assign(sinkDevice);

  // Energy model setup
  BasicEnergySourceHelper energySourceHelper;
  energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(1000.0)); // Each node starts with 1000 J

  EnergySourceContainer sources = energySourceHelper.Install(sensorNodes);

  WifiRadioEnergyModelHelper radioEnergyHelper;
  radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.017));
  radioEnergyHelper.Set("RxCurrentA", DoubleValue(0.017));
  radioEnergyHelper.Set("IdleCurrentA", DoubleValue(0.017));
  radioEnergyHelper.Set("SleepCurrentA", DoubleValue(0.001));

  radioEnergyHelper.Install(sensorDevices, sources);

  // Install application on sensor nodes
  ApplicationContainer sensorApps;
  for (uint32_t i = 0; i < sensorNodes.GetN(); ++i)
  {
    Ptr<Node> node = sensorNodes.Get(i);
    Ptr<SensorNodeApplication> app = CreateObject<SensorNodeApplication>();
    app->SetAttribute("Interval", TimeValue(Seconds(1)));
    app->SetAttribute("PacketSize", UintegerValue(64));
    node->AddApplication(app);
    app->SetStartTime(Seconds(0.0));
    app->SetStopTime(Seconds(totalTime));
  }

  // Sink application
  PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
  ApplicationContainer sinkApp = sinkHelper.Install(sinkNode);
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(totalTime));

  Simulator::Stop(Seconds(totalTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}