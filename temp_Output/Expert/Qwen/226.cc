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

NS_LOG_COMPONENT_DEFINE("SensorNetworkOLSR");

class SensorApplication : public Application
{
public:
  static TypeId GetTypeId();
  SensorApplication();
  virtual ~SensorApplication();

protected:
  virtual void StartApplication();
  virtual void StopApplication();

private:
  void SendPacket();
  uint32_t m_packetSize;
  Time m_interval;
  Ptr<Socket> m_socket;
  Address m_peerAddress;
  EventId m_sendEvent;
};

TypeId
SensorApplication::GetTypeId()
{
  static TypeId tid = TypeId("SensorApplication")
                          .SetParent<Application>()
                          .AddConstructor<SensorApplication>()
                          .AddAttribute("Interval",
                                        "The time between sent packets",
                                        TimeValue(Seconds(1.0)),
                                        MakeTimeAccessor(&SensorApplication::m_interval),
                                        MakeTimeChecker())
                          .AddAttribute("PacketSize",
                                        "Size of the packet to send",
                                        UintegerValue(512),
                                        MakeUintegerAccessor(&SensorApplication::m_packetSize),
                                        MakeUintegerChecker<uint32_t>());
  return tid;
}

SensorApplication::SensorApplication()
    : m_socket(nullptr), m_peerAddress(), m_interval(Seconds(1.0)), m_packetSize(512)
{
}

SensorApplication::~SensorApplication()
{
  m_socket = nullptr;
}

void
SensorApplication::StartApplication()
{
  if (!m_socket)
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    m_socket->Bind();
    m_socket->Connect(m_peerAddress);
  }

  SendPacket();
}

void
SensorApplication::StopApplication()
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
SensorApplication::SendPacket()
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);

  m_sendEvent = Simulator::Schedule(m_interval, &SensorApplication::SendPacket, this);
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  LogComponentEnable("SensorApplication", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(6); // 5 sensors + 1 sink

  WifiMacHelper wifiMac;
  WifiPhyHelper wifiPhy;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(channel.Create());

  wifiMac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(10.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper stack;
  OlsrHelper olsr;
  Ipv4ListRoutingHelper list;
  list.Add(olsr, 10);
  stack.SetRoutingHelper(list);
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  EnergySourceHelper energySourceHelper;
  energySourceHelper.Set("EnergySourceInitialVoltage", DoubleValue(3.0));
  energySourceHelper.Set("BatteryCapacity", DoubleValue(2000.0)); // mAh
  energySourceHelper.Set("VoltageSag", BooleanValue(false));
  energySourceHelper.Set("NominalVoltage", DoubleValue(3.0));
  energySourceHelper.Set("CutoffVoltage", DoubleValue(2.0));

  BasicEnergySourceHelper basicEnergySourceHelper;
  basicEnergySourceHelper.Set("InitialEnergy", DoubleValue(100000)); // Joules

  DeviceEnergyModelHelper deviceEnergyModelHelper;
  deviceEnergyModelHelper.Set("TxCurrentA", DoubleValue(0.017)); // WiFi TX current in A
  deviceEnergyModelHelper.Set("RxCurrentA", DoubleValue(0.015)); // WiFi RX current in A

  EnergySourceContainer sources = energySourceHelper.Install(nodes);
  DeviceEnergyModelContainer deviceModels = deviceEnergyModelHelper.Install(devices, sources);

  uint16_t port = 9;
  for (uint32_t i = 0; i < nodes.GetN() - 1; ++i)
  {
    NodeContainer pair(nodes.Get(i), nodes.Get(nodes.GetN() - 1));
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(nodes.GetN() - 1, 0), port));

    ApplicationContainer sensorApp;
    SensorApplication *sensor = new SensorApplication();
    sensor->SetAttribute("Peer", AddressValue(sinkAddress));
    sensor->SetAttribute("Interval", TimeValue(Seconds(2.0)));
    sensor->SetAttribute("PacketSize", UintegerValue(1024));
    sensorApp.Add(sensor);
    sensorApp.Install(nodes.Get(i));
    sensorApp.Start(Seconds(1.0));
    sensorApp.Stop(Seconds(50.0));
  }

  PacketSinkHelper packetSink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = packetSink.Install(nodes.Get(nodes.GetN() - 1));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(50.0));

  Simulator::Stop(Seconds(50.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}