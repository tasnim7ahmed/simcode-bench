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

  ServerApplication() : m_socket(0) {}
  virtual ~ServerApplication();

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);
  void HandleRead(Ptr<Socket> socket);

  Ptr<Socket> m_socket;
  uint64_t m_totalRx = 0;
};

ServerApplication::~ServerApplication()
{
  m_socket = nullptr;
}

void ServerApplication::StartApplication(void)
{
  if (!m_socket)
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
    m_socket->Bind(local);
    m_socket->SetRecvCallback(MakeCallback(&ServerApplication::HandleRead, this));
  }
}

void ServerApplication::StopApplication(void)
{
  if (m_socket)
  {
    m_socket->Close();
    m_socket = nullptr;
  }
}

void ServerApplication::HandleRead(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
  {
    m_totalRx += packet->GetSize();
    NS_LOG_UNCOND("Server received packet of size " << packet->GetSize() << " bytes. Total received: " << m_totalRx << " bytes.");
  }
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(3); // 3 client nodes

  NodeContainer wifiApNode;
  wifiApNode.Create(1); // 1 server node (acting as AP)

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;

  NetDeviceContainer staDevices;
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("wifi-network")));
  staDevices = wifi.Install(phy, mac, wifiStaNodes);

  NetDeviceContainer apDevice;
  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("wifi-network")));
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

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Install server application on the AP node
  ServerApplication serverApp;
  wifiApNode.Get(0)->AddApplication(&serverApp);
  serverApp.SetStartTime(Seconds(1.0));
  serverApp.SetStopTime(Seconds(10.0));

  // Install UDP echo clients to send packets to the server
  uint16_t port = 9;
  for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
  {
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(apInterface.GetAddress(0), port)));
    onoff.SetConstantRate(DataRate("1kbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer app = onoff.Install(wifiStaNodes.Get(i));
    app.Start(Seconds(2.0));
    app.Stop(Seconds(10.0));
  }

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}