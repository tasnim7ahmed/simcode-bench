#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTcpMultipleClients");

class ServerApp : public Application
{
public:
  ServerApp() {}
  virtual ~ServerApp() {}

  void Setup(Address address, uint16_t port)
  {
    m_local = InetSocketAddress(Ipv4Address::ConvertFrom(address), port);
  }

private:
  virtual void StartApplication(void)
  {
    m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
    m_socket->Bind(m_local);
    m_socket->Listen();
    m_socket->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address &>(),
                               MakeCallback(&ServerApp::HandleAccept, this));
    m_socket->SetRecvCallback(MakeCallback(&ServerApp::HandleRead, this));
  }

  virtual void StopApplication(void)
  {
    if (m_socket)
    {
      m_socket->Close();
      m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    }
  }

  void HandleAccept(Ptr<Socket> s, const Address &from)
  {
    s->SetRecvCallback(MakeCallback(&ServerApp::HandleRead, this));
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      if (packet->GetSize() > 0)
      {
        NS_LOG_UNCOND("Server received: " << packet->GetSize() << " bytes from " 
                                          << InetSocketAddress::ConvertFrom(from).GetIpv4());
      }
    }
  }

  Ptr<Socket> m_socket;
  Address m_local;
};

int main(int argc, char *argv[])
{
  uint32_t nClients = 5;
  double simulationTime = 5.0;
  uint16_t port = 50000;
  uint32_t payloadSize = 100;
  CommandLine cmd;
  cmd.AddValue("nClients", "Number of client nodes", nClients);
  cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue("simulationTime", "Duration (s)", simulationTime);
  cmd.Parse(argc, argv);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(nClients);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211g);
  wifi.SetRemoteStationManager("ns3::AarfWifiManager");

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns3-wifi-tcp");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, wifiStaNodes);

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                               "MinX", DoubleValue(0.0),
                               "MinY", DoubleValue(0.0),
                               "DeltaX", DoubleValue(2.0),
                               "DeltaY", DoubleValue(2.0),
                               "GridWidth", UintegerValue(3),
                               "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNodes);
  mobility.Install(wifiApNode);

  InternetStackHelper stack;
  stack.Install(wifiStaNodes);
  stack.Install(wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

  Ptr<ServerApp> serverApp = CreateObject<ServerApp>();
  serverApp->Setup(apInterface.GetAddress(0), port);
  wifiApNode.Get(0)->AddApplication(serverApp);
  serverApp->SetStartTime(Seconds(0.0));
  serverApp->SetStopTime(Seconds(simulationTime));

  for (uint32_t i = 0; i < nClients; ++i)
  {
    Ptr<Socket> tcpSocket = Socket::CreateSocket(wifiStaNodes.Get(i), TcpSocketFactory::GetTypeId());

    Ptr<Application> clientApp = CreateObject<OnOffApplication>();
    AddressValue remoteAddress(InetSocketAddress(apInterface.GetAddress(0), port));
    clientApp->SetAttribute("Remote", remoteAddress);
    clientApp->SetAttribute("Protocol", StringValue("ns3::TcpSocketFactory"));
    clientApp->SetAttribute("PacketSize", UintegerValue(payloadSize));
    clientApp->SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    clientApp->SetAttribute("MaxBytes", UintegerValue(payloadSize));
    wifiStaNodes.Get(i)->AddApplication(clientApp);
    clientApp->SetStartTime(Seconds(1.0 + i * 0.1));
    clientApp->SetStopTime(Seconds(simulationTime));
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}