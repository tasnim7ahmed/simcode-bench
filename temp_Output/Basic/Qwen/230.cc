#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpWiFiSimulation");

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
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
    m_socket->Bind(local);
    m_socket->SetRecvCallback(MakeCallback(&ServerApplication::HandleRead, this));
    NS_LOG_INFO("Server listening on port 9");
  }

  virtual void StopApplication(void)
  {
    if (m_socket)
    {
      m_socket->Close();
      m_socket = nullptr;
    }
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      NS_LOG_INFO("Server received packet of size " << packet->GetSize() << " from " << from);
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
  wifiApNode.Create(1); // One server node

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;

  NetDeviceContainer staDevices;
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(Ssid("wifi-network")));
  staDevices = wifi.Install(phy, mac, wifiStaNodes);

  NetDeviceContainer apDevice;
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(Ssid("wifi-network")));
  apDevice = wifi.Install(phy, mac, wifiApNode);

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
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

  uint16_t port = 9;

  ApplicationContainer serverApps;
  for (uint32_t i = 0; i < wifiApNode.GetN(); ++i)
  {
    Ptr<Node> node = wifiApNode.Get(i);
    Ptr<ServerApplication> app = CreateObject<ServerApplication>();
    node->AddApplication(app);
    app->SetStartTime(Seconds(1.0));
    app->SetStopTime(Seconds(10.0));
  }

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
  {
    Ptr<Node> node = wifiStaNodes.Get(i);
    Ptr<UdpClient> client = CreateObject<UdpClient>();
    client->SetRemote(apInterface.GetAddress(0), port);
    client->SetAttribute("MaxPackets", UintegerValue(10));
    client->SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client->SetAttribute("PacketSize", UintegerValue(1024));
    node->AddApplication(client);
    client->SetStartTime(Seconds(2.0));
    client->SetStopTime(Seconds(10.0));
  }

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}