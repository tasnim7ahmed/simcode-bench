#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpWiFiSimulation");

class UdpServer : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("UdpServer")
                            .SetParent<Application>()
                            .AddConstructor<UdpServer>();
    return tid;
  }

  UdpServer() {}
  virtual ~UdpServer() {}

protected:
  void DoInitialize() override
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
    m_socket->Bind(local);
    m_socket->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, this));
    NS_LOG_INFO("UDP server socket created and bound.");
  }

  void DoDispose() override
  {
    m_socket->Close();
    Application::DoDispose();
  }

private:
  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      NS_LOG_INFO("Server received a packet of size " << packet->GetSize() << " from " << from);
    }
  }

  Ptr<Socket> m_socket;
};

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  // Enable logging
  LogComponentEnable("UdpWiFiSimulation", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  // Create nodes: 1 server and 3 clients
  NodeContainer nodes;
  nodes.Create(4);

  NodeContainer serverNode = NodeContainer(nodes.Get(0));
  NodeContainer clientNodes = NodeContainer(nodes.Get(1), nodes.Get(2), nodes.Get(3));

  // Setup Wi-Fi
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
  mac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  // Mobility model (Constant Position)
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(5.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  // Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Install UDP Server on node 0
  ApplicationContainer serverApps;
  UdpServer udpServer;
  serverNode.Get(0)->AddApplication(&udpServer);
  udpServer.SetStartTime(Seconds(1.0));
  udpServer.SetStopTime(Seconds(10.0));

  // Install UDP Echo Clients
  UdpEchoClientHelper echoClientHelper(interfaces.GetAddress(0), 9);
  echoClientHelper.SetAttribute("MaxPackets", UintegerValue(5));
  echoClientHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClientHelper.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClientHelper.Install(clientNodes);
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Set up global routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}