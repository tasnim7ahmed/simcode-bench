#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/lora-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;
using namespace lorawan;

NS_LOG_COMPONENT_DEFINE("LoRaWANSensorSinkSimulation");

class SensorApplication : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("SensorApplication")
                            .SetParent<Application>()
                            .AddConstructor<SensorApplication>()
                            .AddAttribute("Interval",
                                          "The interval between packet sends",
                                          TimeValue(Seconds(10)),
                                          MakeTimeAccessor(&SensorApplication::m_interval),
                                          MakeTimeChecker())
                            .AddAttribute("PacketSize",
                                          "Size of the packets sent",
                                          UintegerValue(12),
                                          MakeUintegerAccessor(&SensorApplication::m_packetSize),
                                          MakeUintegerChecker<uint8_t>());
    return tid;
  }

  SensorApplication()
      : m_socket(nullptr), m_peer(), m_interval(Seconds(1)), m_packetSize(12), m_sendEvent()
  {
  }

  virtual ~SensorApplication()
  {
    m_socket = nullptr;
  }

protected:
  void DoInitialize() override
  {
    Application::DoInitialize();
    Simulator::ScheduleNow(&SensorApplication::SendPacket, this);
  }

private:
  void StartApplication() override
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Connect(m_peer);
  }

  void StopApplication() override
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

  void SendPacket()
  {
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->Send(packet);

    NS_LOG_INFO(Simulator::Now().As(Time::S) << " Sensor sent packet to sink. Size: " << m_packetSize);

    m_sendEvent = Simulator::Schedule(m_interval, &SensorApplication::SendPacket, this);
  }

  Ptr<Socket> m_socket;
  Address m_peer;
  Time m_interval;
  uint8_t m_packetSize;
  EventId m_sendEvent;
};

static class SensorApplicationRegistration
{
public:
  SensorApplicationRegistration()
  {
    LogComponentRegister("SensorApplication", LOG_LEVEL_ALL);
    TypeId tid = SensorApplication::GetTypeId();
  }
} g_sensorApplicationRegistration;

void ReceivePacket(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
  {
    NS_LOG_INFO(Simulator::Now().As(Time::S) << " Sink received a packet of size: " << packet->GetSize());
  }
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2); // node 0: sink, node 1: sensor

  // Create LoRa channel
  Ptr<LoRaChannel> channel = CreateObject<LoRaChannel>();

  // Install LoRa PHY and MAC on both nodes
  LoRaPhyHelper phyHelper;
  phyHelper.SetChannel(channel);
  phyHelper.SetDeviceType(LORAWAN_PHY_DEVICE_TYPE_LORAD,
                          nodes.Get(0)); // Downlink capable for sink
  phyHelper.SetDeviceType(LORAWAN_PHY_DEVICE_TYPE_LORAC, nodes.Get(1)); // Sensor node

  Mac48Address macAddressSink = Mac48Address::Allocate();
  Mac48Address macAddressSensor = Mac48Address::Allocate();

  LorawanMacHelper macHelper;
  macHelper.SetAddressGenerator(Mac48AddressGenerator(macAddressSink));
  macHelper.Install(nodes.Get(0)); // Sink
  macHelper.SetAddressGenerator(Mac48AddressGenerator(macAddressSensor));
  macHelper.Install(nodes.Get(1)); // Sensor

  NetDeviceContainer devices = phyHelper.Install(nodes);

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(500.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Sink application setup
  uint16_t sinkPort = 8080;
  UdpServerHelper server(sinkPort);
  ApplicationContainer sinkApp = server.Install(nodes.Get(0));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(100.0));

  // Connect client to sink
  Ptr<Node> sensorNode = nodes.Get(1);
  Ptr<Application> app = CreateObject<SensorApplication>();
  app->SetAttribute("Interval", TimeValue(Seconds(10)));
  app->SetAttribute("PacketSize", UintegerValue(20));
  InetSocketAddress sinkAddr(interfaces.GetAddress(0), sinkPort);
  app->SetPeer(sinkAddr);
  sensorNode->AddApplication(app);
  app->SetStartTime(Seconds(2.0));
  app->SetStopTime(Seconds(100.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}