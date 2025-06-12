#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/lora-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LoRaWANSensorToSink");

class SensorApplication : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("SensorApplication")
      .SetParent<Application>()
      .AddConstructor<SensorApplication>()
      .AddAttribute("Interval", "The interval between packet sends",
                    TimeValue(Seconds(10)),
                    MakeTimeAccessor(&SensorApplication::m_interval),
                    MakeTimeChecker())
      .AddAttribute("PacketSize", "Size of the packet to send",
                    UintegerValue(50),
                    MakeUintegerAccessor(&SensorApplication::m_packetSize),
                    MakeUintegerChecker<uint32_t>());
    return tid;
  }

  SensorApplication() : m_socket(nullptr), m_peer(), m_sendEvent(), m_running(false) {}
  virtual ~SensorApplication() { }

protected:
  void DoStart() override
  {
    NS_LOG_LOGIC("Starting application");
    m_running = true;
    m_sendEvent = Simulator::ScheduleNow(&SensorApplication::SendPacket, this);
  }

  void DoStop() override
  {
    NS_LOG_LOGIC("Stopping application");
    m_running = false;
    if (m_sendEvent.IsRunning())
    {
      Simulator::Cancel(m_sendEvent);
    }
  }

private:
  void SendPacket()
  {
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    NS_LOG_INFO("At time " << Simulator::Now().GetSeconds() << "s sending packet of size " << m_packetSize);
    m_socket->Send(packet);

    if (m_running)
    {
      m_sendEvent = Simulator::Schedule(m_interval, &SensorApplication::SendPacket, this);
    }
  }

  Ptr<Socket> m_socket;
  Address m_peer;
  Time m_interval;
  uint32_t m_packetSize;
  EventId m_sendEvent;
  bool m_running;
};

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  NodeContainer sensorNode = NodeContainer(nodes.Get(0));
  NodeContainer sinkNode = NodeContainer(nodes.Get(1));

  // Setup LoRa channel
  Ptr<LoRaChannel> channel = CreateObject<LoRaChannel>();

  // Setup LoRa PHY and MAC
  Ptr<LoRaPhy> senderPhy = CreateObject<LoRaPhy>();
  senderPhy->SetChannel(channel);
  senderPhy->SetMobilityModel(CreateObject<ConstantPositionMobilityModel>());
  sensorNode.Get(0)->AggregateObject(senderPhy);

  Ptr<LoRaMac> senderMac = CreateObject<ClassCMacLayer>();
  senderMac->SetPhy(senderPhy);
  sensorNode.Get(0)->AggregateObject(senderMac);

  Ptr<LoRaPhy> receiverPhy = CreateObject<LoRaPhy>();
  receiverPhy->SetChannel(channel);
  receiverPhy->SetMobilityModel(CreateObject<ConstantPositionMobilityModel>());
  sinkNode.Get(0)->AggregateObject(receiverPhy);

  Ptr<LoRaMac> receiverMac = CreateObject<ClassCMacLayer>();
  receiverMac->SetPhy(receiverPhy);
  sinkNode.Get(0)->AggregateObject(receiverMac);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(channel->GetNetDevices());

  // Install sink application (UDP server)
  UdpServerHelper udpServer(80);
  ApplicationContainer sinkApp = udpServer.Install(sinkNode);
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(100.0));

  // Install sensor application
  ApplicationContainer sensorApp;
  SensorApplicationHelper sensorAppHelper;
  sensorAppHelper.SetAttribute("Interval", TimeValue(Seconds(10)));
  sensorAppHelper.SetAttribute("PacketSize", UintegerValue(50));
  sensorApp = sensorAppHelper.Install(sensorNode);
  sensorApp.Start(Seconds(0.0));
  sensorApp.Stop(Seconds(100.0));

  // Set peer address for UDP client
  Ptr<UdpClient> client = DynamicCast<UdpClient>(sensorApp.Get(0));
  if (client)
  {
    client->SetRemote(interfaces.GetAddress(1), 80);
  }

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}