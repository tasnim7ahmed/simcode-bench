#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/energy-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SensorNetworkSimulation");

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
  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_packetSize;
  DataRate m_dataRate;
  EventId m_sendEvent;
  bool m_running;
};

TypeId
SensorApplication::GetTypeId()
{
  static TypeId tid = TypeId("SensorApplication")
                          .SetParent<Application>()
                          .SetGroupName("Tutorial")
                          .AddConstructor<SensorApplication>();
  return tid;
}

SensorApplication::SensorApplication()
    : m_socket(0),
      m_peer(),
      m_packetSize(1024),
      m_dataRate(1024),
      m_running(false)
{
}

SensorApplication::~SensorApplication()
{
}

void
SensorApplication::StartApplication()
{
  NS_LOG_FUNCTION(this);

  if (m_socket == 0)
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);

    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
    m_socket->Bind(local);
  }

  m_socket->Connect(m_peer);
  m_running = true;
  SendPacket();
}

void
SensorApplication::StopApplication()
{
  NS_LOG_FUNCTION(this);
  m_running = false;

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
  NS_LOG_FUNCTION(this);

  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);

  Time interval = Seconds(5.0);
  m_sendEvent = Simulator::Schedule(interval, &SensorApplication::SendPacket, this);
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  LogComponentEnable("SensorNetworkSimulation", LOG_LEVEL_INFO);
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

  OlsrHelper olsr;
  InternetStackHelper internet;
  internet.SetRoutingHelper(olsr);
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Energy Model Setup
  BasicEnergySourceHelper energySourceHelper;
  energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(1000));
  EnergySourceContainer sources = energySourceHelper.Install(nodes);

  DeviceEnergyModelContainer deviceModels;
  for (uint32_t i = 0; i < nodes.GetN(); ++i)
  {
    Ptr<DeviceEnergyModel> model = CreateObject<WifiRadioEnergyModel>();
    model->SetNode(nodes.Get(i));
    model->SetEnergySource(sources.Get(i));
    deviceModels.Add(model);
    sources.Get(i)->AppendDeviceEnergyModel(model);
    nodes.Get(i)->AggregateObject(sources.Get(i));
  }

  // Sink node listens on port 9
  uint16_t port = 9;
  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(nodes.Get(0));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(50.0));

  // Sensors send to sink
  for (uint32_t i = 1; i < nodes.GetN(); ++i)
  {
    SensorApplication *app = new SensorApplication();
    app->SetAttribute("Remote", AddressValue(InetSocketAddress(interfaces.GetAddress(0), port)));
    ApplicationContainer clientApp(app);
    clientApp.Install(nodes.Get(i));
    app->SetStartTime(Seconds(1.0));
    app->SetStopTime(Seconds(50.0));
  }

  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("sensor-network.tr");
  wifiPhy.EnableAsciiAll(stream);

  Simulator::Stop(Seconds(50.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}