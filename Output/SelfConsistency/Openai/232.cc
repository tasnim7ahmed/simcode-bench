#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTcpMultiClientExample");

class TcpServerApp : public Application
{
public:
  TcpServerApp() : m_socket(0), m_recvBytes(0) {}
  virtual ~TcpServerApp() { m_socket = 0; }

  void Setup(Address address, uint16_t port)
  {
    m_local = InetSocketAddress(Ipv4Address::GetAny(), port);
    m_address = address;
    m_port = port;
  }

private:
  virtual void StartApplication(void)
  {
    if (!m_socket)
    {
      m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
      m_socket->Bind(m_local);
      m_socket->Listen();
      m_socket->SetAcceptCallback(
          MakeNullCallback<bool, Ptr<Socket>, const Address &>(),
          MakeCallback(&TcpServerApp::HandleAccept, this));
    }
  }

  virtual void StopApplication(void)
  {
    if (m_socket)
    {
      m_socket->Close();
      m_socket = 0;
    }
  }

  void HandleAccept(Ptr<Socket> s, const Address &from)
  {
    s->SetRecvCallback(MakeCallback(&TcpServerApp::HandleRead, this));
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      if (packet->GetSize() == 0)
        break;
      m_recvBytes += packet->GetSize();
      NS_LOG_UNCOND("Server received " << packet->GetSize() << " bytes from " 
                                       << InetSocketAddress::ConvertFrom(from).GetIpv4());
    }
  }

  Ptr<Socket> m_socket;
  Address m_local;
  Address m_address;
  uint16_t m_port;
  uint32_t m_recvBytes;
};

int main(int argc, char *argv[])
{
  uint32_t nClients = 3;
  double simulationTime = 5.0; // seconds
  uint32_t payloadSize = 1024; // bytes

  CommandLine cmd;
  cmd.AddValue("nClients", "Number of client nodes", nClients);
  cmd.Parse(argc, argv);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(nClients);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  // Wi-Fi configuration
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
  wifi.SetRemoteStationManager("ns3::AarfWifiManager");

  WifiMacHelper mac;
  Ssid ssid = Ssid("simple-wifi");
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

  // Mobility
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNodes);
  mobility.Install(wifiApNode);

  // Stack and IP assignment
  InternetStackHelper stack;
  stack.Install(wifiStaNodes);
  stack.Install(wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

  // Server: install custom TCP server app on AP
  uint16_t serverPort = 50000;
  Ptr<TcpServerApp> serverApp = CreateObject<TcpServerApp>();
  serverApp->Setup(apInterface.GetAddress(0), serverPort);
  wifiApNode.Get(0)->AddApplication(serverApp);
  serverApp->SetStartTime(Seconds(0.0));
  serverApp->SetStopTime(Seconds(simulationTime));

  // Clients: TCP socket-based BulkSendApplication
  for (uint32_t i = 0; i < nClients; ++i)
  {
    BulkSendHelper clientHelper("ns3::TcpSocketFactory",
                                InetSocketAddress(apInterface.GetAddress(0), serverPort));
    clientHelper.SetAttribute("MaxBytes", UintegerValue(payloadSize));
    ApplicationContainer clientApp = clientHelper.Install(wifiStaNodes.Get(i));
    clientApp.Start(Seconds(1.0 + 0.1 * i)); // Stagger client starts
    clientApp.Stop(Seconds(simulationTime));
  }

  // Enable logging
  LogComponentEnable("WifiTcpMultiClientExample", LOG_LEVEL_INFO);

  Simulator::Stop(Seconds(simulationTime + 1));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}