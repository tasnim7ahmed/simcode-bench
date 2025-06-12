#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiCustomUdp");

class UdpClient : public Application
{
public:
  UdpClient();
  virtual ~UdpClient();

  void Setup(Address address, uint32_t packetSize, uint32_t numPackets, DataRate dataRate);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  void SendPacket(void);
  void ReceivedReply(Ptr<Socket> socket);
  void SocketErr(Ptr<Socket> socket);

  Address m_peerAddress;
  uint32_t m_packetSize;
  uint32_t m_numPackets;
  DataRate m_dataRate;
  Ptr<Socket> m_socket;
  EventId m_sendEvent;
  uint32_t m_packetsSent;
};

UdpClient::UdpClient() : m_socket(nullptr), m_sendEvent(), m_packetsSent(0) {}

UdpClient::~UdpClient()
{
  m_socket = nullptr;
}

void UdpClient::Setup(Address address, uint32_t packetSize, uint32_t numPackets, DataRate dataRate)
{
  m_peerAddress = address;
  m_packetSize = packetSize;
  m_numPackets = numPackets;
  m_dataRate = dataRate;
}

void UdpClient::StartApplication(void)
{
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  if (m_socket == 0)
  {
    NS_FATAL_ERROR("Failed to create socket");
  }
  m_socket->Bind();
  m_socket->Connect(m_peerAddress);
  m_socket->SetRecvCallback(MakeCallback(&UdpClient::ReceivedReply, this));
  m_socket->SetErrCallback(MakeCallback(&UdpClient::SocketErr, this));

  m_packetsSent = 0;
  SendPacket();
}

void UdpClient::StopApplication(void)
{
  Simulator::Cancel(m_sendEvent);
  if (m_socket != 0)
  {
    m_socket->Close();
  }
}

void UdpClient::SendPacket(void)
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);

  m_packetsSent++;
  if (m_packetsSent < m_numPackets)
  {
    Time tNext(Seconds(m_packetSize * 8.0 / m_dataRate.GetBitRate()));
    m_sendEvent = Simulator::Schedule(tNext, &UdpClient::SendPacket, this);
  }
}

void UdpClient::ReceivedReply(Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet = socket->RecvFrom(from);
}

void UdpClient::SocketErr(Ptr<Socket> socket)
{
  NS_LOG_INFO("Socket error on socket " << socket);
}

class UdpServer : public Application
{
public:
  UdpServer();
  virtual ~UdpServer();

  void Setup(uint16_t port);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  void HandleRead(Ptr<Socket> socket);
  void SocketErr(Ptr<Socket> socket);
  void AcceptConnection(Ptr<Socket> socket, const Address &from);

  uint16_t m_port;
  Ptr<Socket> m_socket;
  Address m_localAddress;
};

UdpServer::UdpServer() : m_port(0), m_socket(nullptr) {}

UdpServer::~UdpServer()
{
  m_socket = nullptr;
}

void UdpServer::Setup(uint16_t port)
{
  m_port = port;
}

void UdpServer::StartApplication(void)
{
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  if (m_socket == 0)
  {
    NS_FATAL_ERROR("Failed to create socket");
  }
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
  m_socket->Bind(local);
  m_socket->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, this));
  m_socket->SetErrCallback(MakeCallback(&UdpServer::SocketErr, this));
  m_socket->SetAcceptCallback(MakeCallback(&UdpServer::AcceptConnection, this));
  m_socket->Listen();
}

void UdpServer::StopApplication(void)
{
  if (m_socket != 0)
  {
    m_socket->Close();
  }
}

void UdpServer::HandleRead(Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet;
  while ((packet = socket->RecvFrom(from)))
  {
    uint32_t receivedSize = packet->GetSize();
    NS_LOG_INFO("Received " << receivedSize << " bytes from " << from);
  }
}

void UdpServer::SocketErr(Ptr<Socket> socket)
{
  NS_LOG_INFO("Socket error on socket " << socket);
}

void UdpServer::AcceptConnection(Ptr<Socket> socket, const Address &from)
{
  NS_LOG_INFO("Accept connection from " << from);
}

int main(int argc, char *argv[])
{
  bool verbose = false;
  uint32_t packetSize = 1024;
  uint32_t numPackets = 100;
  DataRate dataRate("1Mbps");
  double simulationTime = 10.0;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue("packetSize", "Size of echo packets", packetSize);
  cmd.AddValue("numPackets", "Number of packets generated", numPackets);
  cmd.AddValue("dataRate", "The data rate", dataRate);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse(argc, argv);

  if (verbose)
  {
    LogComponentEnable("WifiCustomUdp", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_ALL);
    LogComponentEnable("UdpServer", LOG_LEVEL_ALL);
  }

  NodeContainer apNode;
  apNode.Create(1);

  NodeContainer clientNodes;
  clientNodes.Create(1);

  NodeContainer serverNodes;
  serverNodes.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconGeneration", BooleanValue(true),
              "BeaconInterval", TimeValue(Seconds(1.0)));

  NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer clientDevice = wifi.Install(phy, mac, clientNodes);
  NetDeviceContainer serverDevice = wifi.Install(phy, mac, serverNodes);

  InternetStackHelper stack;
  stack.Install(apNode);
  stack.Install(clientNodes);
  stack.Install(serverNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
  Ipv4InterfaceContainer clientInterface = address.Assign(clientDevice);
  Ipv4InterfaceContainer serverInterface = address.Assign(serverDevice);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;
  UdpServerHelper serverHelper(port);
  ApplicationContainer serverApp = serverHelper.Install(serverNodes.Get(0));
  Ptr<UdpServer> server = DynamicCast<UdpServer>(serverApp.Get(0));
  server->Setup(port);
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(simulationTime));

  UdpClientHelper clientHelper(serverInterface.GetAddress(0), port);
  ApplicationContainer clientApp = clientHelper.Install(clientNodes.Get(0));
  Ptr<UdpClient> client = DynamicCast<UdpClient>(clientApp.Get(0));
  client->Setup(serverInterface.GetAddress(0), packetSize, numPackets, dataRate);

  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(simulationTime));

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}