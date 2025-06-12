#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiClientServerSimulation");

class ServerApplication : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("ServerApplication")
      .SetParent<Application>()
      .AddConstructor<ServerApplication>();
    return tid;
  }

  ServerApplication() {}
  virtual ~ServerApplication() {}

private:
  virtual void StartApplication(void)
  {
    TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 8080);
    m_socket->Bind(local);
    m_socket->Listen();
    m_socket->SetAcceptCallback(MakeCallback(&ServerApplication::HandleAccept, this), MakeCallback(&ServerApplication::HandleAccept, this));
    NS_LOG_INFO("Server started listening on port 8080");
  }

  virtual void StopApplication(void)
  {
    if (m_socket)
    {
      m_socket->Close();
      m_socket = nullptr;
    }
  }

  void HandleAccept(Ptr<Socket> s, const Address &from)
  {
    NS_LOG_INFO("Accepted connection from " << from);
    s->SetRecvCallback(MakeCallback(&ServerApplication::HandleRead, this));
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      NS_LOG_INFO("Received packet of size " << packet->GetSize() << " from " << from);
    }
  }

  Ptr<Socket> m_socket;
};

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(3); // Three client nodes
  NodeContainer wifiApNode;
  wifiApNode.Create(1); // One server node as AP

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;

  NetDeviceContainer staDevices;
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(Ssid("wifi-network")),
              "ActiveProbing", BooleanValue(false));
  staDevices = wifi.Install(phy, mac, wifiStaNodes);

  NetDeviceContainer apDevices;
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(Ssid("wifi-network")));
  apDevices = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(5.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNodes);
  mobility.Install(wifiApNode);

  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");

  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign(apDevices);
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign(staDevices);

  ApplicationContainer serverApps;
  for (uint32_t i = 0; i < wifiApNode.GetN(); ++i)
  {
    serverApps.Add(CreateObject<ServerApplication>());
    wifiApNode.Get(i)->AddApplication(serverApps.Get(i));
  }
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  uint32_t packetSize = 1024;
  uint32_t maxPackets = 1;
  Time interval = Seconds(1.0);
  for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
  {
    Ptr<Node> clientNode = wifiStaNodes.Get(i);
    Ptr<Socket> sock = Socket::CreateSocket(clientNode, TcpSocketFactory::GetTypeId());
    InetSocketAddress remote = InetSocketAddress(apInterface.GetAddress(0), 8080);
    OnOffHelper client(sock, remote);
    client.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));
    client.SetAttribute("MaxBytes", UintegerValue(packetSize * maxPackets));
    ApplicationContainer app = client.Install(clientNode);
    app.Start(Seconds(2.0 + i));
    app.Stop(Seconds(10.0));
  }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}