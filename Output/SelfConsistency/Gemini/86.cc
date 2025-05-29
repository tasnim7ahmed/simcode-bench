#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/socket.h"
#include "ns3/wifi-module.h"

#include <fstream>
#include <iostream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiSenderReceiver");

class MyApp : public Application {
public:
  MyApp();
  virtual ~MyApp();

  void Setup(Address address, uint32_t packetSize, uint32_t maxPackets,
             Time packetInterval);

private:
  virtual void StartApplication() override;
  virtual void StopApplication() override;

  void SendPacket();
  void ScheduleTx();
  void ReceivedPacket(Ptr<Socket> socket);

  Address m_peer;
  Ptr<Socket> m_socket;
  uint32_t m_packetSize;
  uint32_t m_maxPackets;
  Time m_packetInterval;
  uint32_t m_packetsSent;
};

MyApp::MyApp()
    : m_peer(), m_socket(nullptr), m_packetSize(0), m_maxPackets(0),
      m_packetInterval(Seconds(0)), m_packetsSent(0) {}

MyApp::~MyApp() { m_socket = nullptr; }

void MyApp::Setup(Address address, uint32_t packetSize, uint32_t maxPackets,
                  Time packetInterval) {
  m_peer = address;
  m_packetSize = packetSize;
  m_maxPackets = maxPackets;
  m_packetInterval = packetInterval;
}

void MyApp::StartApplication() {
  m_socket = Socket::CreateSocket(GetNode(), TypeId::LookupByName("ns3::UdpSocketFactory"));
  m_socket->Bind();
  m_socket->Connect(m_peer);
  m_socket->SetRecvCallback(MakeCallback(&MyApp::ReceivedPacket, this));

  m_packetsSent = 0;
  ScheduleTx();
}

void MyApp::StopApplication() {
  Simulator::Cancel(m_socket->GetRecvCallback());
  if (m_socket) {
    m_socket->Close();
    m_socket->Dispose();
    m_socket = nullptr;
  }
}

void MyApp::SendPacket() {
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);

  if (++m_packetsSent < m_maxPackets) {
    ScheduleTx();
  }
}

void MyApp::ScheduleTx() {
  Simulator::Schedule(m_packetInterval, &MyApp::SendPacket, this);
}

void MyApp::ReceivedPacket(Ptr<Socket> socket) {
  Address senderAddress;
  Ptr<Packet> packet = socket->RecvFrom(senderAddress);
  Time arrivalTime = Simulator::Now();

  std::cout << "Received packet from " << senderAddress
            << " at time " << arrivalTime.GetSeconds() << "s" << std::endl;
}

static void CourseChange(std::string path, double oldValue, double newValue) {
  std::cout << "Course change from " << oldValue << " to " << newValue
            << std::endl;
}

int main(int argc, char *argv[]) {
  bool verbose = false;
  uint32_t nWifi = 2;
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue("nWifi", "Number of wifi STA devices", nWifi);
  cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue("tracing", "Enable pcap tracing", tracing);

  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
  }

  NodeContainer wifiNodes;
  wifiNodes.Create(nWifi);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
  Ssid ssid = Ssid("ns-3-ssid");

  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing",
              BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, wifiNodes.Get(1));

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, wifiNodes.Get(0));

  InternetStackHelper stack;
  stack.Install(wifiNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces;
  interfaces = address.Assign(apDevices);
  interfaces = address.Assign(staDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  Ptr<Socket> recvSink =
      Socket::CreateSocket(wifiNodes.Get(1), tid); // Receiver is node 1
  InetSocketAddress local = InetSocketAddress(interfaces.GetAddress(1), 8080);
  recvSink->Bind(local);

  Ptr<MyApp> app = CreateObject<MyApp>();
  app->Setup(InetSocketAddress(interfaces.GetAddress(0), 8080), 1024, 1000,
            Seconds(1.0));
  wifiNodes.Get(0)->AddApplication(app); // Sender is node 0
  app->SetStartTime(Seconds(1.0));
  app->SetStopTime(Seconds(10.0));

  Simulator::Stop(Seconds(11.0));

  if (tracing) {
    phy.EnablePcap("wifi-sender-receiver", apDevices);
  }

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}