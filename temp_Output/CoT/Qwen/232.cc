#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiClientServerSimulation");

class ServerApplication : public Application
{
public:
  static TypeId GetTypeId();
  ServerApplication();
  virtual ~ServerApplication();

protected:
  virtual void StartApplication();
  virtual void StopApplication();

private:
  void HandleRead(Ptr<Socket> socket);

  Ptr<Socket> m_socket;
};

TypeId ServerApplication::GetTypeId()
{
  static TypeId tid = TypeId("ServerApplication")
    .SetParent<Application>()
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
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
    m_socket->Bind(local);
    m_socket->Listen();
    m_socket->SetAcceptCallback(MakeCallback(&ServerApplication::HandleRead, this),
                                MakeCallback(&ServerApplication::HandleRead, this));
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
    NS_LOG_INFO("Received packet size: " << packet->GetSize() << " from " << from);
  }
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(5); // 5 clients
  NodeContainer wifiApNode;
  wifiApNode.Create(1); // 1 server

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");

  WifiMacHelper mac;
  mac.SetType("ns3::StaWifiMac");
  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, wifiStaNodes);

  mac.SetType("ns3::ApWifiMac");
  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(5.0),
                                "GridWidth", UintegerValue(5),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNodes);
  mobility.Install(wifiApNode);

  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");

  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign(apDevices);

  uint16_t port = 9;

  ServerApplication serverApp;
  wifiApNode.Get(0)->AddApplication(&serverApp);
  serverApp.SetStartTime(Seconds(1.0));
  serverApp.SetStopTime(Seconds(10.0));

  for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
  {
    TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
    Ptr<Socket> clientSocket = Socket::CreateSocket(wifiStaNodes.Get(i), tid);
    InetSocketAddress remote = InetSocketAddress(apInterface.GetAddress(0), port);
    clientSocket->Connect(remote);

    Simulator::Schedule(Seconds(2.0 + i), &Socket::Send, clientSocket, Create<Packet>(1024), 0);
  }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}