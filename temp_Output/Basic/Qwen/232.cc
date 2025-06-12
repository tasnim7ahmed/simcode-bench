#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpWifiSimulation");

class ServerApplication : public Application
{
public:
  static TypeId GetTypeId();
  ServerApplication();
  virtual ~ServerApplication();

private:
  virtual void StartApplication();
  virtual void StopApplication();
  void HandleRead(Ptr<Socket> socket);

  Ptr<Socket> m_socket;
};

TypeId ServerApplication::GetTypeId()
{
  static TypeId tid = TypeId("ServerApplication")
    .SetParent<Application>()
    .SetGroupName("Tutorial")
    .AddConstructor<ServerApplication>();
  return tid;
}

ServerApplication::ServerApplication()
{
  m_socket = nullptr;
}

ServerApplication::~ServerApplication()
{
  m_socket = nullptr;
}

void ServerApplication::StartApplication()
{
  if (!m_socket)
  {
    TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 5000);
    m_socket->Bind(local);
    m_socket->Listen();
    m_socket->SetRecvCallback(MakeCallback(&ServerApplication::HandleRead, this));
  }
}

void ServerApplication::StopApplication()
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
    NS_LOG_INFO("Received packet of size " << packet->GetSize() << " from " << from);
  }
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(4); // 1 server, 3 clients

  WifiMacHelper wifiMac;
  WifiPhyHelper wifiPhy;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);

  wifiMac.SetType("ns3::AdhocWifiMac");
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  NetDeviceContainer devices = wifi.Install(phy, wifiMac, nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Place all nodes at the same position for simplicity
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(0.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(1),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  // Install server application on node 0
  ServerApplication serverApp;
  nodes.Get(0)->AddApplication(&serverApp);
  serverApp.SetStartTime(Seconds(1.0));
  serverApp.SetStopTime(Seconds(10.0));

  // Install client applications on other nodes
  for (uint32_t i = 1; i < nodes.GetN(); ++i)
  {
    OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(0), 5000));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("100kbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(512));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer app = onoff.Install(nodes.Get(i));
    app.Start(Seconds(2.0));
    app.Stop(Seconds(10.0));
  }

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}