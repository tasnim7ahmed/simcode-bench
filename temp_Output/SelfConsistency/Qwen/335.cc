#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTcpRandomWalkSimulation");

class TcpClientApplication : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("TcpClientApplication")
                            .SetParent<Application>()
                            .SetGroupName("Tutorial")
                            .AddConstructor<TcpClientApplication>();
    return tid;
  }

  TcpClientApplication();
  virtual ~TcpClientApplication();

protected:
  void StartApplication(void);
  void StopApplication(void);

private:
  uint32_t m_packetCount;
  uint32_t m_totalPackets;
  uint32_t m_packetSize;
  Ipv4Address m_serverIp;
  uint16_t m_serverPort;
  Ptr<Socket> m_socket;

  void SendPacket(void);
};

TcpClientApplication::TcpClientApplication()
    : m_packetCount(0), m_totalPackets(10), m_packetSize(1024),
      m_serverIp("10.1.1.1"), m_serverPort(9), m_socket(nullptr)
{
}

void TcpClientApplication::StartApplication(void)
{
  m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
  InetSocketAddress remote = InetSocketAddress(m_serverIp, m_serverPort);
  m_socket->Connect(remote);
  Simulator::Schedule(Seconds(0.0), &TcpClientApplication::SendPacket, this);
}

void TcpClientApplication::StopApplication(void)
{
  if (m_socket)
  {
    m_socket->Close();
    m_socket = nullptr;
  }
}

void TcpClientApplication::SendPacket(void)
{
  if (m_packetCount < m_totalPackets)
  {
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->Send(packet);
    NS_LOG_INFO("Sent packet number " << m_packetCount + 1 << " of size " << m_packetSize << " bytes");
    m_packetCount++;
    Simulator::Schedule(Seconds(1.0), &TcpClientApplication::SendPacket, this);
  }
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  // Enable logging
  LogComponentEnable("TcpClientApplication", LOG_LEVEL_INFO);

  // Create nodes: Client (STA) and Access Point (AP)
  NodeContainer wifiStaNode;
  wifiStaNode.Create(1);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  // Setup Wi-Fi
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_5GHz);

  WifiMacHelper mac;
  Ssid ssid = Ssid("wifi-tcp-simulation");

  // STA configuration
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(mac, wifiStaNode);

  // AP configuration
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(mac, wifiApNode);

  // Mobility models
  MobilityHelper mobility;

  // Random walk for client
  mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
  mobility.Install(wifiStaNode);

  // Stationary model for AP
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiApNode);

  // Internet stack
  InternetStackHelper stack;
  stack.InstallAll();

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apInterfaces;
  apInterfaces = address.Assign(apDevices);

  // Server application on AP (port 9)
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory",
                                   InetSocketAddress(Ipv4Address::GetAny(), 9));
  ApplicationContainer serverApp = packetSinkHelper.Install(wifiApNode);
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(15.0));

  // Client application on STA
  ApplicationContainer clientApp;
  Ptr<TcpClientApplication> app = CreateObject<TcpClientApplication>();
  wifiStaNode.Get(0)->AddApplication(app);
  app->SetStartTime(Seconds(1.0));
  app->SetStopTime(Seconds(11.0));

  // Enable PCAP tracing
  wifi.EnablePcap("tcp_wifi_simulation_ap", apDevices.Get(0));
  wifi.EnablePcap("tcp_wifi_simulation_sta", staDevices.Get(0));

  // Run simulation
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}