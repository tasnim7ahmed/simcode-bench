#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/energy-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WSNEnergySimulation");

class SensorNodeApplication : public Application
{
public:
  static TypeId GetTypeId(void);
  SensorNodeApplication();
  virtual ~SensorNodeApplication();

protected:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

private:
  void SendPacket(void);
  uint32_t m_packetSize;
  Time m_interval;
  Ptr<Socket> m_socket;
  Address m_sinkAddress;
  EventId m_sendEvent;
};

TypeId
SensorNodeApplication::GetTypeId(void)
{
  static TypeId tid = TypeId("SensorNodeApplication")
                        .SetParent<Application>()
                        .AddConstructor<SensorNodeApplication>()
                        .AddAttribute("PacketSize", "Size of packets sent.",
                                      UintegerValue(100),
                                      MakeUintegerAccessor(&SensorNodeApplication::m_packetSize),
                                      MakeUintegerChecker<uint32_t>())
                        .AddAttribute("Interval", "Interval between packets.",
                                      TimeValue(Seconds(5.0)),
                                      MakeTimeAccessor(&SensorNodeApplication::m_interval),
                                      MakeTimeChecker());
  return tid;
}

SensorNodeApplication::SensorNodeApplication()
    : m_socket(nullptr), m_sendEvent(), m_sinkAddress()
{
}

SensorNodeApplication::~SensorNodeApplication()
{
  m_socket = nullptr;
}

void
SensorNodeApplication::StartApplication(void)
{
  if (m_socket == nullptr)
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    InetSocketAddress sinkAddr = InetSocketAddress(Ipv4Address("10.1.1.1"), 9);
    m_sinkAddress = sinkAddr;
  }

  Simulator::ScheduleNow(&SensorNodeApplication::SendPacket, this);
}

void
SensorNodeApplication::StopApplication(void)
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
SensorNodeApplication::SendPacket(void)
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  if (m_socket->SendTo(packet, 0, m_sinkAddress) >= 0)
  {
    NS_LOG_DEBUG("Sent packet from node " << GetNode()->GetId());
  }

  m_sendEvent = Simulator::Schedule(m_interval, &SensorNodeApplication::SendPacket, this);
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  LogComponentEnable("WSNEnergySimulation", LOG_LEVEL_INFO);
  LogComponentEnable("SensorNodeApplication", LOG_LEVEL_INFO);

  // Simulation parameters
  uint32_t numNodesPerSide = 5; // Grid size: 5x5 = 25 nodes
  double gridSpacing = 50.0;    // meters
  double simulationTime = 86400; // seconds (simulate for one day)

  NodeContainer sensorNodes;
  sensorNodes.Create(numNodesPerSide * numNodesPerSide);

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

  // Sink node position in the center
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector((numNodesPerSide - 1) * gridSpacing / 2, (numNodesPerSide - 1) * gridSpacing / 2, 0));
  mobility.SetPositionAllocator(positionAlloc);
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
  InternetStackHelper internet;
  internet.Install(NodeContainer(sensorNodes, sinkNode));

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer sensorInterfaces = ipv4.Assign(sensorDevices);
  Ipv4InterfaceContainer sinkInterface = ipv4.Assign(sinkDevice);

  // Energy model setup
  BasicEnergySourceHelper energySource;
  energySource.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(1000)); // 1000 Joules

  DeviceEnergyModelContainer deviceEnergyModels;
  deviceEnergyModels = energySource.Install(sensorNodes, sensorDevices);
  energySource.Install(sinkNode, sinkDevice); // Sink has unlimited energy

  // Install applications
  uint16_t port = 9;
  UdpServerHelper server(port);
  ApplicationContainer sinkApp = server.Install(sinkNode.Get(0));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(simulationTime));

  ApplicationContainer apps;
  for (uint32_t i = 0; i < sensorNodes.GetN(); ++i)
  {
    Ptr<Node> node = sensorNodes.Get(i);
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    Ipv4InterfaceAddress iaddr = ipv4->GetInterface(1)->GetAddress(0);
    Ipv4Address nodeIp = iaddr.GetLocal();

    NS_LOG_INFO("Node " << node->GetId() << " IP: " << nodeIp);

    SensorNodeApplicationHelper helper;
    helper.SetAttribute("Interval", TimeValue(Seconds(10)));
    helper.SetAttribute("PacketSize", UintegerValue(64));

    ApplicationContainer app = helper.Install(node);
    app.Get(0)->GetObject<SensorNodeApplication>()->SetSinkAddress(InetSocketAddress(sinkInterface.GetAddress(0), port));
    app.Start(Seconds(1.0 + i));
    app.Stop(Seconds(simulationTime));
  }

  // Flow monitor to check throughput
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  monitor->CheckForLostPackets();
  monitor->SerializeToXmlFile("wsn-energy.flowmon", false, false);

  Simulator::Destroy();
  return 0;
}