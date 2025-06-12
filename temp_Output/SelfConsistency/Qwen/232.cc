#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpWifiSimulation");

class ServerApplication : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("ServerApplication")
                            .SetParent<Application>()
                            .SetGroupName("Tutorial")
                            .AddConstructor<ServerApplication>();
    return tid;
  }

  ServerApplication() : m_socket(0) {}
  virtual ~ServerApplication();

protected:
  void DoInitialize() override
  {
    Application::DoInitialize();
    if (m_socket == nullptr)
    {
      TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
      m_socket = Socket::CreateSocket(GetNode(), tid);
      InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
      m_socket->Bind(local);
      m_socket->Listen();
    }
    m_socket->SetAcceptCallback(MakeCallback(&ServerApplication::HandleAccept, this), MakeCallback(&ServerApplication::HandleAccept, this));
  }

private:
  void StartApplication() override {}
  void StopApplication() override {}

  void HandleAccept(Ptr<Socket> s, const Address &from)
  {
    NS_LOG_INFO("Connection accepted from " << from);
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

  // Create nodes: one server and multiple clients
  NodeContainer serverNode;
  serverNode.Create(1);

  NodeContainer clientNodes;
  clientNodes.Create(3);

  NodeContainer allNodes = NodeContainer(serverNode, clientNodes);

  // Install internet stack
  InternetStackHelper stack;
  stack.Install(allNodes);

  // Setup Wi-Fi
  WifiMacHelper wifiMac;
  WifiPhyHelper wifiPhy;
  WifiHelper wifi;

  wifi.SetStandard(WIFI_STANDARD_80211b);

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("wifi-network")));
  NetDeviceContainer clientDevices = wifi.Install(phy, wifiMac, clientNodes);

  wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("wifi-network")));
  NetDeviceContainer serverDevice = wifi.Install(phy, wifiMac, serverNode);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(clientDevices, serverDevice));

  // Set up mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(allNodes);

  // Install server application
  ServerApplication serverApp;
  serverNode.Get(0)->AddApplication(&serverApp);
  serverApp.SetStartTime(Seconds(1.0));
  serverApp.SetStopTime(Seconds(10.0));

  // Install client applications
  uint16_t port = 9;
  for (uint32_t i = 0; i < clientNodes.GetN(); ++i)
  {
    Ptr<Node> client = clientNodes.Get(i);
    TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
    Ptr<Socket> sock = Socket::CreateSocket(client, tid);
    InetSocketAddress remote(interfaces.GetAddress(i), port);
    sock->Connect(remote);

    // Send small data
    Ptr<Packet> pkt = Create<Packet>(1024); // 1KB packet
    sock->Send(pkt);

    // Close connection after sending
    Simulator::Schedule(Seconds(2.0), &Socket::Close, sock);
  }

  // Enable logging
  LogComponentEnable("TcpWifiSimulation", LOG_LEVEL_INFO);
  LogComponentEnable("ServerApplication", LOG_LEVEL_INFO);

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}