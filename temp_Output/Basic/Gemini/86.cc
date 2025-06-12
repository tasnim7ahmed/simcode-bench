#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/point-to-point-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiSenderReceiver");

class MyApp : public Application
{
public:
  MyApp();
  virtual ~MyApp();

  void Setup(Address address, uint32_t packetSize, DataRate dataRate);

  void SetPacketSize(uint32_t packetSize) { m_packetSize = packetSize; }
  void SetRemote(Address remote) { m_peer = remote; }
  void SetInterval(Time interval) { m_interval = interval; }

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  void SendPacket(void);

  Address m_peer;
  uint32_t m_packetSize;
  DataRate m_dataRate;
  EventId m_sendEvent;
  bool m_running;
  Time m_interval;
  SocketPtr m_socket;
  uint32_t m_packetsSent;
};

MyApp::MyApp() : m_running(false), m_packetsSent(0)
{
}

MyApp::~MyApp()
{
  m_socket = nullptr;
}

void MyApp::Setup(Address address, uint32_t packetSize, DataRate dataRate)
{
  m_peer = address;
  m_packetSize = packetSize;
  m_dataRate = dataRate;
}

void MyApp::StartApplication(void)
{
  m_running = true;
  m_packetsSent = 0;
  m_socket = Socket::CreateSocket(GetNode(), TypeId::LookupByName("ns3::UdpSocketFactory"));
  m_socket->Bind();
  m_socket->Connect(m_peer);
  SendPacket();
}

void MyApp::StopApplication(void)
{
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

void MyApp::SendPacket(void)
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);

  if (++m_packetsSent < 1000 && m_running)
  {
    m_sendEvent = Simulator::Schedule(m_interval, &MyApp::SendPacket, this);
  }
}

class MyReceiver : public Application
{
public:
  MyReceiver();
  virtual ~MyReceiver();

  void Setup(uint16_t port);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  void HandleRead(Ptr<Socket> socket);
  void ReceivePacket(Ptr<Socket> socket);

  uint16_t m_port;
  SocketPtr m_socket;
  Address m_local;
  Time m_firstRx;
  uint64_t m_bytesReceived;
  bool m_first;
};

MyReceiver::MyReceiver() : m_first(true)
{
}

MyReceiver::~MyReceiver()
{
    m_socket = nullptr;
}

void MyReceiver::Setup(uint16_t port)
{
  m_port = port;
}

void MyReceiver::StartApplication(void)
{
  m_socket = Socket::CreateSocket(GetNode(), TypeId::LookupByName("ns3::UdpSocketFactory"));
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
  m_socket->Bind(local);
  m_socket->SetRecvCallback(MakeCallback(&MyReceiver::HandleRead, this));
  m_local = local;
  m_bytesReceived = 0;
}

void MyReceiver::StopApplication(void)
{
    if (m_socket) {
        m_socket->Close();
    }
}

void MyReceiver::HandleRead(Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet = socket->RecvFrom(from);
  if (m_first)
  {
    m_firstRx = Simulator::Now();
    m_first = false;
  }
  m_bytesReceived += packet->GetSize();
  Time arrival_time = Simulator::Now();
  Time sent_time;
  packet->RemoveAllPacketTags();
  packet->RemoveAllByteTags();

  NS_LOG_INFO(Simulator::Now().AsDouble() << " " << packet->GetSize() << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4() << " delay " << (arrival_time - sent_time).AsDouble());
}

int main(int argc, char *argv[])
{
  bool verbose = false;
  uint32_t packetSize = 1024;
  double interval = 0.001; // seconds
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue("packetSize", "Size of the packets generated. Default: 1024", packetSize);
  cmd.AddValue("interval", "Interval between packets in seconds. Default: 0.001", interval);
  cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue("tracing", "Enable pcap tracing", tracing);

  cmd.Parse(argc, argv);

  if (verbose)
  {
    LogComponentEnable("WifiSenderReceiver", LOG_LEVEL_INFO);
  }

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Create();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, nodes.Get(1));

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, nodes.Get(0));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(apDevices);
  interfaces = address.Assign(staDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;

  Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));

  Ptr<MyApp> app = CreateObject<MyApp>();
  app->Setup(sinkAddress, packetSize, DataRate("5Mb/s"));
  app->SetStartTime(Seconds(1.));
  app->SetStopTime(Seconds(10.));
  nodes.Get(0)->AddApplication(app);

  Ptr<MyReceiver> sink = CreateObject<MyReceiver>();
  sink->Setup(port);
  sink->SetStartTime(Seconds(1.0));
  sink->SetStopTime(Seconds(10.0));
  nodes.Get(1)->AddApplication(sink);

  if (tracing)
  {
    phy.EnablePcap("wifi-sender-receiver", apDevices);
  }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}