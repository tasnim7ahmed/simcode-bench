#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/energy-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessSensorNetworkSimulation");

class SensorNodeApplication : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("SensorNodeApplication")
      .SetParent<Application>()
      .AddConstructor<SensorNodeApplication>()
      .AddAttribute("DestinationAddress", "The address of the sink node",
                    Ipv4AddressValue(),
                    MakeIpv4AddressAccessor(&SensorNodeApplication::m_sinkAddress),
                    MakeIpv4AddressChecker())
      .AddAttribute("PacketSize", "The size of packets sent in bytes.",
                    UintegerValue(128),
                    MakeUintegerAccessor(&SensorNodeApplication::m_packetSize),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("SendInterval", "Interval between packet sends in seconds.",
                    TimeValue(Seconds(1)),
                    MakeTimeAccessor(&SensorNodeApplication::m_sendInterval),
                    MakeTimeChecker());
    return tid;
  }

  SensorNodeApplication() : m_socket(nullptr) {}
  virtual ~SensorNodeApplication() { delete m_socket; }

private:
  void StartApplication(void)
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Bind();
    m_socket->Connect(InetSocketAddress(m_sinkAddress, 9));
    SendPacket();
  }

  void StopApplication(void)
  {
    if (m_sendEvent.IsRunning())
      Simulator::Cancel(m_sendEvent);
    if (m_socket)
    {
      m_socket->Close();
    }
  }

  void SendPacket(void)
  {
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->Send(packet);

    NS_LOG_INFO(Simulator::Now().As(Time::S) << " Node " << GetNode()->GetId()
                                             << " sent packet to sink");

    m_sendEvent = Simulator::Schedule(m_sendInterval, &SensorNodeApplication::SendPacket, this);
  }

  Ipv4Address m_sinkAddress;
  uint32_t m_packetSize;
  Time m_sendInterval;
  EventId m_sendEvent;
  Ptr<Socket> m_socket;
};

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  LogComponentEnable("WirelessSensorNetworkSimulation", LOG_LEVEL_INFO);

  // Simulation parameters
  uint32_t numNodesPerSide = 5;
  double gridSpacing = 50.0;
  double simulationDuration = 1000.0;

  // Setup nodes
  uint32_t totalNodes = numNodesPerSide * numNodesPerSide;
  NodeContainer sensorNodes;
  sensorNodes.Create(totalNodes);

  NodeContainer sinkNode;
  sinkNode.Create(1);

  // Mobility setup
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(gridSpacing),
                                "DeltaY", DoubleValue(gridSpacing),
                                "GridWidth", UintegerValue(numNodesPerSide),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(sensorNodes);

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue((numNodesPerSide / 2) * gridSpacing),
                                "MinY", DoubleValue((numNodesPerSide / 2) * gridSpacing),
                                "DeltaX", DoubleValue(0),
                                "DeltaY", DoubleValue(0),
                                "GridWidth", UintegerValue(1),
                                "LayoutType", StringValue("RowFirst"));
  mobility.Install(sinkNode);

  // WiFi setup
  WifiMacHelper wifiMac;
  WifiPhyHelper wifiPhy;
  WifiHelper wifi;

  wifi.SetStandard(WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");

  wifiMac.SetType("ns3::AdhocWifiMac");
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  NetDeviceContainer sensorDevices = wifi.Install(phy, wifiMac, sensorNodes);
  NetDeviceContainer sinkDevice = wifi.Install(phy, wifiMac, sinkNode);

  // Internet stack
  InternetStackHelper stack;
  stack.Install(sensorNodes);
  stack.Install(sinkNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer sensorInterfaces = address.Assign(sensorDevices);
  Ipv4InterfaceContainer sinkInterface = address.Assign(sinkDevice);

  Ipv4Address sinkIp = sinkInterface.GetAddress(0, 0);

  // Energy model setup
  BasicEnergySourceHelper energySourceHelper;
  energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(10000)); // 10 kJ

  DeviceEnergyModelContainer deviceEnergyModels;
  WifiRadioEnergyModelHelper radioEnergyHelper;
  for (uint32_t i = 0; i < sensorNodes.GetN(); ++i)
  {
    EnergySourceContainer sources = energySourceHelper.Install(sensorNodes.Get(i));
    deviceEnergyModels.Add(radioEnergyHelper.Install(sensorDevices.Get(i), sources.Get(0)));
  }

  // Install applications
  ApplicationContainer apps;
  for (uint32_t i = 0; i < sensorNodes.GetN(); ++i)
  {
    Ptr<Node> node = sensorNodes.Get(i);
    if (node == sinkNode.Get(0)) continue;

    Ptr<SensorNodeApplication> app = CreateObject<SensorNodeApplication>();
    app->SetAttribute("DestinationAddress", Ipv4AddressValue(sinkIp));
    node->AddApplication(app);
    app->SetStartTime(Seconds(0.0));
    app->SetStopTime(Seconds(simulationDuration));
  }

  // Sink application (packet sink)
  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
  ApplicationContainer sinkApp = sink.Install(sinkNode);
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(simulationDuration));

  // Connect energy depletion event
  for (uint32_t i = 0; i < sensorNodes.GetN(); ++i)
  {
    Config::ConnectWithoutContext("/NodeList/" + std::to_string(sensorNodes.Get(i)->GetId()) +
                                  "/DeviceList/0/$ns3::WifiNetDevice/RemoteStationManager/PowerManagement/DcfManager/EdcaTxopN/Queue/Dequeue",
      MakeCallback([](Ptr<const Packet> p) {
        for (auto& entry : NodeList::GetList())
        {
          bool allDead = true;
          for (uint32_t j = 0; j < entry->GetNDevices(); ++j)
          {
            Ptr<NetDevice> dev = entry->GetDevice(j);
            if (dev->GetObject<WifiNetDevice>())
            {
              auto energyModel = DynamicCast<DeviceEnergyModel>(dev->GetObject<DeviceEnergyModel>());
              if (energyModel && energyModel->GetCurrentA() > 0)
              {
                allDead = false;
                break;
              }
            }
          }
          if (allDead)
          {
            NS_LOG_INFO(Simulator::Now().As(Time::S) << " All nodes are out of energy. Stopping simulation.");
            Simulator::Stop();
            return;
          }
        }
      }));
  }

  Simulator::Stop(Seconds(simulationDuration));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}