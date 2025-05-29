#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/energy-module.h"
#include "ns3/applications-module.h"
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WsnEnergyDepletionExample");

class WsnEnergyManager : public Object
{
public:
  WsnEnergyManager(const std::vector<Ptr<BasicEnergySource>> &sources)
      : m_sources(sources) {}
  void Setup()
  {
    Simulator::Schedule(Seconds(0.1), &WsnEnergyManager::CheckEnergy, this);
  }
  void CheckEnergy()
  {
    bool anyAlive = false;
    for (auto source : m_sources)
    {
      if (source->GetRemainingEnergy() > 0)
      {
        anyAlive = true;
        break;
      }
    }
    if (!anyAlive)
    {
      NS_LOG_UNCOND("All nodes are depleted of energy. Stopping simulation at " << Simulator::Now().GetSeconds() << "s");
      Simulator::Stop();
      return;
    }
    Simulator::Schedule(Seconds(0.1), &WsnEnergyManager::CheckEnergy, this);
  }

private:
  std::vector<Ptr<BasicEnergySource>> m_sources;
};

int main(int argc, char *argv[])
{
  uint32_t gridWidth = 5;
  uint32_t gridHeight = 5;
  double gridSpacing = 20.0; // meters
  uint32_t numSensors = gridWidth * gridHeight;
  double initialEnergyJ = 1.0;

  CommandLine cmd;
  cmd.AddValue("gridWidth", "Number of nodes per row", gridWidth);
  cmd.AddValue("gridHeight", "Number of nodes per column", gridHeight);
  cmd.AddValue("spacing", "Distance between nodes", gridSpacing);
  cmd.AddValue("initialEnergyJ", "Initial energy (Joules)", initialEnergyJ);
  cmd.Parse(argc, argv);

  NodeContainer sensorNodes;
  sensorNodes.Create(numSensors);
  NodeContainer sinkNode;
  sinkNode.Create(1);

  // Mobility: Grid for sensors, center for sink
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                               "MinX", DoubleValue(0.0),
                               "MinY", DoubleValue(0.0),
                               "DeltaX", DoubleValue(gridSpacing),
                               "DeltaY", DoubleValue(gridSpacing),
                               "GridWidth", UintegerValue(gridWidth),
                               "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(sensorNodes);

  MobilityHelper sinkMobility;
  sinkMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  sinkMobility.Install(sinkNode);
  // Place sink at grid center
  Ptr<MobilityModel> sinkPos = sinkNode.Get(0)->GetObject<MobilityModel>();
  double sinkX = gridSpacing * (gridWidth - 1) / 2.0;
  double sinkY = gridSpacing * (gridHeight - 1) / 2.0;
  sinkPos->SetPosition(Vector(sinkX, sinkY, 0.0));

  // WiFi: use 802.11b (Lowest Rate), Ad Hoc
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());
  WifiMacHelper mac;
  mac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices;
  devices = wifi.Install(phy, mac, sensorNodes);
  NetDeviceContainer sinkDevice;
  sinkDevice = wifi.Install(phy, mac, sinkNode);

  // Energy model
  BasicEnergySourceHelper energySourceHelper;
  energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(initialEnergyJ));

  // One energy source per node (sensors only)
  EnergySourceContainer energySources = energySourceHelper.Install(sensorNodes);

  WifiRadioEnergyModelHelper radioEnergyHelper;
  DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install(devices, energySources);

  // Logging energy depletion of each node
  for (uint32_t i = 0; i < numSensors; ++i)
  {
    Ptr<BasicEnergySource> energySrc = DynamicCast<BasicEnergySource>(energySources.Get(i));
    energySrc->TraceConnectWithoutContext("RemainingEnergy",
      MakeCallback([i](double oldVal, double newVal) {
        if (newVal <= 0 && oldVal > 0)
          NS_LOG_UNCOND("Node " << i << " depleted energy at " << Simulator::Now().GetSeconds() << "s");
      }));
  }

  // WiFi energy consumption for sink (optional, sink is mains powered, set infinite energy)
  Ptr<BasicEnergySource> sinkEnergy = CreateObject<BasicEnergySource>();
  sinkEnergy->SetInitialEnergy(1e9);
  sinkNode.Get(0)->AggregateObject(sinkEnergy);
  radioEnergyHelper.Install(sinkDevice, EnergySourceContainer(sinkEnergy));

  // Networking stack
  InternetStackHelper internet;
  internet.Install(sensorNodes);
  internet.Install(sinkNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer sensorIfaces = address.Assign(devices);
  Ipv4InterfaceContainer sinkIface = address.Assign(sinkDevice);

  // UDP traffic from each sensor to the sink
  uint16_t sinkPort = 50000;
  Address sinkAddress(InetSocketAddress(sinkIface.GetAddress(0), sinkPort));
  PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps = sinkHelper.Install(sinkNode);
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(10000)); // Arbitrarily large, will be stopped by Simulator::Stop()

  // Generate traffic apps and schedule only for energy-not-depleted nodes
  double appStart = 1.0;
  double appStop = 10000.0;
  double packetInterval = 2.0; // seconds
  uint32_t packetSize = 32;   // bytes

  for (uint32_t i = 0; i < numSensors; ++i)
  {
    Ptr<Node> node = sensorNodes.Get(i);
    Ptr<Socket> srcSocket = Socket::CreateSocket(node, UdpSocketFactory::GetTypeId());
    srcSocket->Connect(sinkAddress);

    // Periodic packet sending with energy check
    auto sendFunc = [srcSocket, i, energySources, packetSize, packetInterval] () mutable {
      Ptr<BasicEnergySource> src = DynamicCast<BasicEnergySource>(energySources.Get(i));
      if (src->GetRemainingEnergy() > 0)
      {
        Ptr<Packet> p = Create<Packet>(packetSize);
        srcSocket->Send(p);
        Simulator::Schedule(Seconds(packetInterval), sendFunc);
      }
    };
    Simulator::Schedule(Seconds(appStart + i * 0.01), sendFunc);
  }

  // Stop if all nodes deplete energy
  std::vector<Ptr<BasicEnergySource>> energyVector;
  for (uint32_t i = 0; i < numSensors; ++i)
    energyVector.push_back(DynamicCast<BasicEnergySource>(energySources.Get(i)));
  Ptr<WsnEnergyManager> manager = CreateObject<WsnEnergyManager>(energyVector);
  manager->Setup();

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}