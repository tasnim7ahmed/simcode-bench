#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lorawan-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

// Application to send periodic UDP packets
class SensorApp : public Application
{
public:
  SensorApp() : m_socket(0), m_peer(), m_interval(Seconds(20)), m_count(10), m_sent(0) {}
  void Setup(Address peer, uint32_t count, Time interval)
  {
    m_peer = peer;
    m_count = count;
    m_interval = interval;
  }
private:
  virtual void StartApplication()
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Connect(m_peer);
    SendPacket();
  }
  virtual void StopApplication()
  {
    if (m_socket) m_socket->Close();
  }
  void SendPacket()
  {
    Ptr<Packet> packet = Create<Packet>(20);
    m_socket->Send(packet);
    ++m_sent;
    if (m_sent < m_count)
      Simulator::Schedule(m_interval, &SensorApp::SendPacket, this);
  }
  Ptr<Socket> m_socket;
  Address m_peer;
  Time m_interval;
  uint32_t m_count;
  uint32_t m_sent;
};

class SinkApp : public Application
{
public:
  SinkApp() : m_socket(0) {}
  void Setup(Address address)
  {
    m_local = address;
  }
private:
  virtual void StartApplication()
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Bind(m_local);
    m_socket->SetRecvCallback(MakeCallback(&SinkApp::HandleRead, this));
  }
  virtual void StopApplication()
  {
    if (m_socket) m_socket->Close();
  }
  void HandleRead(Ptr<Socket> socket)
  {
    Address from;
    Ptr<Packet> packet;
    while ((packet = socket->RecvFrom(from)))
    {
      NS_LOG_UNCOND("Sink received " << packet->GetSize() << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4());
    }
  }
  Ptr<Socket> m_socket;
  Address m_local;
};

int main(int argc, char *argv[])
{
  LogComponentEnable("SinkApp", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer endDevices;
  endDevices.Create(2); // 0: sensor, 1: sink

  Ptr<Node> sensor = endDevices.Get(0);
  Ptr<Node> sink = endDevices.Get(1);

  // Create a single gateway and a NetworkServer and JoinServer
  NodeContainer gateways, networkServer, joinServer;
  gateways.Create(1);
  networkServer.Create(1);
  joinServer.Create(1);

  // Setup mobility (static)
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> alloc = CreateObject<ListPositionAllocator>();
  alloc->Add(Vector(0, 0, 0));     // Sensor
  alloc->Add(Vector(10, 0, 0));    // Sink
  alloc->Add(Vector(5, 5, 5));     // Gateway
  mobility.SetPositionAllocator(alloc);
  mobility.Install(endDevices);
  mobility.Install(gateways);

  // Setup LoRa
  LoRaChannelHelper channelHelper;
  Ptr<LoRaChannel> channel = channelHelper.Create();
  LoRaPhyHelper phyHelper;
  phyHelper.SetChannel(channel);

  // End device helpers
  LorawanMacHelper macHelper;
  LorawanHelper helper;
  phyHelper.SetChannel(channel);

  NetDeviceContainer endDevicesNetDevices = helper.Install(phyHelper, macHelper, endDevices);
  NetDeviceContainer gwNetDevices = helper.InstallGateway(phyHelper, gateways);
  helper.EnableNetworkServer(networkServer);
  helper.EnableJoinServer(joinServer);

  // Install Internet
  InternetStackHelper stack;
  stack.Install(endDevices);
  stack.Install(gateways);
  stack.Install(networkServer);
  stack.Install(joinServer);

  // Assign IPv4 addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifEndDev = ipv4.Assign(endDevicesNetDevices);
  ipv4.NewNetwork();
  Ipv4InterfaceContainer ifGw = ipv4.Assign(gwNetDevices);

  // Application configuration
  uint16_t sinkPort = 4000;
  Address sinkAddress(InetSocketAddress(ifEndDev.GetAddress(1), sinkPort));

  // Install Sensor App
  Ptr<SensorApp> sensorApp = CreateObject<SensorApp>();
  sensorApp->Setup(sinkAddress, 10, Seconds(20));
  sensor->AddApplication(sensorApp);
  sensorApp->SetStartTime(Seconds(1));
  sensorApp->SetStopTime(Seconds(201));

  // Install Sink App
  Ptr<SinkApp> sinkApp = CreateObject<SinkApp>();
  Address localAddress(InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  sinkApp->Setup(localAddress);
  sink->AddApplication(sinkApp);
  sinkApp->SetStartTime(Seconds(0.5));
  sinkApp->SetStopTime(Seconds(202));

  Simulator::Stop(Seconds(205));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}