#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/energy-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SensorNetworkSimulation");

class SensorApplication : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("SensorApplication")
      .SetParent<Application>()
      .SetGroupName("Tutorial")
      .AddConstructor<SensorApplication>()
      .AddAttribute("Interval", "The time between packets",
                    TimeValue(Seconds(5)),
                    MakeTimeAccessor(&SensorApplication::m_interval),
                    MakeTimeChecker())
      .AddAttribute("DestinationAddress", "The destination address",
                    AddressValue(),
                    MakeAddressAccessor(&SensorApplication::m_destAddress),
                    MakeAddressChecker());
    return tid;
  }

  SensorApplication() : m_socket(nullptr) {}
  virtual ~SensorApplication();

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  void SendPacket(void);

  Time m_interval;
  Address m_destAddress;
  Ptr<Socket> m_socket;
};

SensorApplication::~SensorApplication()
{
  m_socket = nullptr;
}

void SensorApplication::StartApplication(void)
{
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  m_socket->Connect(m_destAddress);

  Simulator::Schedule(Seconds(1.0), &SensorApplication::SendPacket, this);
}

void SensorApplication::StopApplication(void)
{
  if (m_socket)
    {
      m_socket->Close();
    }
}

void SensorApplication::SendPacket(void)
{
  Ptr<Packet> packet = Create<Packet>(1024); // 1KB packet
  m_socket->Send(packet);

  Simulator::Schedule(m_interval, &SensorApplication::SendPacket, this);
}

static class SensorApplicationRegistrationClass
{
public:
  SensorApplicationRegistrationClass()
  {
    LogComponentRegister("SensorApplication", LOG_LEVEL_ALL);
  }
} SensorApplicationRegistrationInstance;

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  uint32_t numSensors = 10;
  nodes.Create(numSensors);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper wifiPhy;
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiHelper wifi;
  wifi.SetRemoteStationManager("ns3::AarfWifiManager");

  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
  wifiMac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(10.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(5),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper internet;
  OlsrHelper olsr;
  Ipv4ListRoutingHelper list;
  list.Add(olsr, 10);
  internet.SetRoutingHelper(list);
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Energy Model Setup
  BasicEnergySourceHelper energySource;
  energySource.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(100.0));

  WifiRadioEnergyModelHelper radioEnergyHelper;
  DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install(devices, energySource.Install(nodes));

  // Install applications on all sensor nodes except sink (node 0)
  uint16_t port = 9;
  for (uint32_t i = 1; i < numSensors; ++i)
    {
      SensorApplicationHelper appHelper;
      appHelper.SetAttribute("Interval", TimeValue(Seconds(5)));
      appHelper.SetAttribute("DestinationAddress", AddressValue(InetSocketAddress(interfaces.GetAddress(0), port)));

      ApplicationContainer apps = appHelper.Install(nodes.Get(i));
      apps.Start(Seconds(1.0));
      apps.Stop(Seconds(50.0));
    }

  // UDP Sink on central node (node 0)
  PacketSinkHelper sink("ns3::UdpSocketFactory",
                        InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = sink.Install(nodes.Get(0));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(50.0));

  Simulator::Stop(Seconds(50.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}