#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lorawan-module.h"
#include "ns3/propagation-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class UdpSinkLogger : public Application
{
public:
  UdpSinkLogger() {}
  virtual ~UdpSinkLogger() {}
  void Setup(Address address, uint16_t port)
  {
    m_port = port;
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
    m_socket->Bind(local);
    m_socket->SetRecvCallback(MakeCallback(&UdpSinkLogger::HandleRead, this));
  }
protected:
  virtual void StartApplication() override
  {
    // Already setup in Setup()
  }
  virtual void StopApplication() override
  {
    if (m_socket)
    {
      m_socket->Close();
    }
  }
private:
  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      uint8_t *buf = new uint8_t[packet->GetSize()+1];
      packet->CopyData(buf, packet->GetSize());
      buf[packet->GetSize()] = '\0';
      NS_LOG_UNCOND("[" << Simulator::Now().GetSeconds() << "s] Sink received " << packet->GetSize() << " bytes: \"" << buf << "\" from " << InetSocketAddress::ConvertFrom(from).GetIpv4());
      delete[] buf;
    }
  }
  Ptr<Socket> m_socket;
  uint16_t m_port;
};

int main(int argc, char *argv[])
{
  LogComponentEnable("UdpSinkLogger", LOG_LEVEL_INFO);

  uint32_t nDevices = 2; // 1 sensor, 1 sink
  uint16_t appPort = 5000;
  Time appStart = Seconds(2.0);
  Time appStop = Seconds(30.0);
  double interval = 5.0; // seconds

  NodeContainer endDevices;
  endDevices.Create(nDevices);

  NodeContainer gateways;
  gateways.Create(1);

  // Mobility: static positions
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0, 0, 0)); // Sensor
  positionAlloc->Add(Vector(50, 0, 0)); // Sink
  mobility.SetPositionAllocator(positionAlloc);
  mobility.Install(endDevices);

  MobilityHelper gwMobility;
  gwMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  gwMobility.Install(gateways);
  gateways.Get(0)->AggregateObject(CreateObject<ConstantPositionMobilityModel>());
  gateways.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(25, 0, 0));

  LorawanHelper lorawan;
  LoraHelper lora;

  // Channel
  Ptr<LogDistancePropagationLossModel> loss = CreateObject<LogDistancePropagationLossModel>();
  loss->SetPathLossExponent(3.76);
  loss->SetReference(1, 7.7);
  Ptr<PropagationDelayModel> delay = CreateObject<ConstantSpeedPropagationDelayModel>();
  Ptr<Channel> channel = CreateObject<SingleModelSpectrumChannel>();
  channel->SetPropagationLossModel(loss);
  channel->SetPropagationDelayModel(delay);

  // Devices
  lorawan.EnableWifi(false);
  NetDeviceContainer endDevicesNetDev = lorawan.Install(lora, endDevices, channel);
  NetDeviceContainer gatewaysNetDev = lorawan.InstallGateway(lora, gateways, channel);

  // EndDevices: one as sensor, one as sink
  Mac16Address sinkMac = Mac16Address::Allocate();

  // Configure all EndDevices as Class A
  lorawan.EnableSchedulers(true);
  lorawan.EnableDutyCycle(true);

  // Internet stack and IP assignment
  InternetStackHelper internet;
  internet.Install(endDevices);
  internet.Install(gateways);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer endDevIpIfaces = ipv4.Assign(endDevicesNetDev);
  Ipv4InterfaceContainer gwIpIfaces = ipv4.Assign(gatewaysNetDev);

  // Applications
  // Sensor node (node 0)
  Ptr<Node> sensorNode = endDevices.Get(0);
  Ptr<Node> sinkNode = endDevices.Get(1);
  Address sinkAddress = InetSocketAddress(endDevIpIfaces.GetAddress(1), appPort);

  // UDP client on sensor node
  Ptr<Socket> sourceSocket = Socket::CreateSocket(sensorNode, UdpSocketFactory::GetTypeId());
  sourceSocket->Connect(sinkAddress);

  // Sensor application sends periodically
  class PeriodicUdpSender : public Application
  {
  public:
    PeriodicUdpSender() : m_interval(Seconds(5.0)), m_running(false), m_count(0) {}
    void Setup(Ptr<Socket> socket, Address address, Time interval, Time stopTime)
    {
      m_socket = socket;
      m_peer = address;
      m_interval = interval;
      m_stopTime = stopTime;
    }
    virtual void StartApplication()
    {
      m_running = true;
      SendPacket();
    }
    virtual void StopApplication()
    {
      m_running = false;
      if (m_socket)
      {
        m_socket->Close();
      }
    }
  private:
    void SendPacket()
    {
      if (!m_running) return;
      std::ostringstream msg;
      msg << "SensorData " << m_count << " @ " << Simulator::Now().GetSeconds() << "s";
      Ptr<Packet> packet = Create<Packet>((uint8_t*)msg.str().c_str(), msg.str().size());
      m_socket->Send(packet);
      ++m_count;
      if (Simulator::Now() + m_interval < m_stopTime)
      {
        Simulator::Schedule(m_interval, &PeriodicUdpSender::SendPacket, this);
      }
    }
    Ptr<Socket> m_socket;
    Address m_peer;
    Time m_interval;
    Time m_stopTime;
    uint32_t m_count;
    bool m_running;
  };

  Ptr<PeriodicUdpSender> senderApp = CreateObject<PeriodicUdpSender>();
  senderApp->Setup(sourceSocket, sinkAddress, Seconds(interval), appStop);
  sensorNode->AddApplication(senderApp);
  senderApp->SetStartTime(appStart);
  senderApp->SetStopTime(appStop);

  // UDP sink on sink node
  Ptr<UdpSinkLogger> sinkApp = CreateObject<UdpSinkLogger>();
  sinkApp->Setup(Address(), appPort);
  sinkNode->AddApplication(sinkApp);
  sinkApp->SetStartTime(appStart);
  sinkApp->SetStopTime(appStop);

  // Routing: minimal L3 functionality for LoRaWAN
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(appStop + Seconds(1.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}