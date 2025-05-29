#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/energy-module.h"
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WsnEnergyDepletionExample");

class DataGenerator : public Application
{
public:
  DataGenerator() : m_socket(0), m_sendInterval(Seconds(2.0)), m_packetSize(50) {}
  void SetRemote(Address address) { m_peer = address; }
  void SetSendInterval(Time interval) { m_sendInterval = interval; }
  void SetPacketSize(uint32_t size) { m_packetSize = size; }
  void SetEnergySource(Ptr<BasicEnergySource> es) { m_energySource = es; }
  void StopWhenDepleted(bool stop) { m_stopWhenDepleted = stop; }

protected:
  virtual void StartApplication()
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Connect(m_peer);
    ScheduleTx();
  }

  virtual void StopApplication()
  {
    if (m_socket)
      m_socket->Close();
  }

  void ScheduleTx()
  {
    if (!m_energySource || m_energySource->GetRemainingEnergy() <= 0)
      return;
    Simulator::Schedule(m_sendInterval, &DataGenerator::SendPacket, this);
  }

  void SendPacket()
  {
    if (!m_energySource || m_energySource->GetRemainingEnergy() <= 0)
      return;
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->Send(packet);
    ScheduleTx();
  }

private:
  Ptr<Socket> m_socket;
  Address m_peer;
  Time m_sendInterval;
  uint32_t m_packetSize;
  Ptr<BasicEnergySource> m_energySource;
  bool m_stopWhenDepleted;
};

void CheckEnergy(const std::vector<Ptr<BasicEnergySource>>& sources, bool &allDepleted, Time interval)
{
  allDepleted = true;
  for (auto const& es : sources)
  {
    if (es->GetRemainingEnergy() > 0)
    {
      allDepleted = false;
      break;
    }
  }
  if (!allDepleted)
  {
    Simulator::Schedule(interval, &CheckEnergy, sources, std::ref(allDepleted), interval);
  }
  else
  {
    NS_LOG_UNCOND("All nodes depleted energy! Stopping simulation at " << Simulator::Now().GetSeconds() << "s");
    Simulator::Stop();
  }
}

int main(int argc, char *argv[])
{
  uint32_t gridWidth = 5;
  uint32_t gridHeight = 5;
  double distance = 20.0;
  double initialEnergyJ = 0.5;
  double txPower_mW = 20;

  CommandLine cmd;
  cmd.AddValue("gridWidth", "Number of nodes in row", gridWidth);
  cmd.AddValue("gridHeight", "Number of nodes in column", gridHeight);
  cmd.AddValue("initialEnergyJ", "Initial energy (J)", initialEnergyJ);
  cmd.Parse(argc, argv);

  uint32_t numSensors = gridWidth * gridHeight;
  uint32_t numNodes = numSensors + 1; // extra node for sink

  NodeContainer sensorNodes;
  sensorNodes.Create(numSensors);

  NodeContainer sinkNode;
  sinkNode.Create(1);

  NodeContainer allNodes;
  allNodes.Add(sensorNodes);
  allNodes.Add(sinkNode);

  // WiFi configuration
  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, allNodes);

  InternetStackHelper stack;
  stack.Install(allNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Mobility: Grid layout for sensors, place sink at center
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
    "MinX", DoubleValue(0.0),
    "MinY", DoubleValue(0.0),
    "DeltaX", DoubleValue(distance),
    "DeltaY", DoubleValue(distance),
    "GridWidth", UintegerValue(gridWidth),
    "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(sensorNodes);

  Ptr<ListPositionAllocator> sinkPosAlloc = CreateObject<ListPositionAllocator>();
  double centerX = 0.5 * distance * (gridWidth - 1);
  double centerY = 0.5 * distance * (gridHeight - 1);
  sinkPosAlloc->Add(Vector(centerX, centerY, 0.0));
  MobilityHelper sinkMob;
  sinkMob.SetPositionAllocator(sinkPosAlloc);
  sinkMob.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  sinkMob.Install(sinkNode);

  // Energy Model
  BasicEnergySourceHelper energySourceHelper;
  energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(initialEnergyJ));

  EnergySourceContainer energySources = energySourceHelper.Install(sensorNodes);
  std::vector<Ptr<BasicEnergySource>> esVec;
  for (uint32_t i = 0; i < energySources.GetN(); ++i)
    esVec.push_back(DynamicCast<BasicEnergySource>(energySources.Get(i)));

  WifiRadioEnergyModelHelper radioEnergyHelper;
  radioEnergyHelper.Set("TxCurrentA", DoubleValue(txPower_mW / 3.0 / 3.7)); // = mW/Volt
  radioEnergyHelper.Set("RxCurrentA", DoubleValue(0.017));
  DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install(devices.GetN() - 1, devices.GetN() - 1, energySources);

  // Sink: UdpServer listening at fixed port
  uint16_t port = 9888;
  UdpServerHelper server(port);
  ApplicationContainer sinkApp = server.Install(sinkNode.Get(0));
  sinkApp.Start(Seconds(0));
  sinkApp.Stop(Seconds(1e5));

  // Sensors: Each sends data to the sink
  ApplicationContainer allSensorApps;
  for (uint32_t i = 0; i < numSensors; ++i)
  {
    Ptr<DataGenerator> gen = CreateObject<DataGenerator>();
    gen->SetRemote(InetSocketAddress(interfaces.GetAddress(numNodes-1), port));
    gen->SetSendInterval(Seconds(2.0));
    gen->SetPacketSize(50);
    gen->SetEnergySource(esVec.at(i));
    sensorNodes.Get(i)->AddApplication(gen);
    gen->SetStartTime(Seconds(1.0+UniformVariable().GetValue(0.0,0.2))); // slightly staggered start
    gen->SetStopTime(Seconds(1e5));
    allSensorApps.Add(gen);
  }

  // Periodic check for total network energy depletion
  bool allDepleted = false;
  Simulator::Schedule(Seconds(1), &CheckEnergy, esVec, std::ref(allDepleted), Seconds(1));

  // Logging
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}