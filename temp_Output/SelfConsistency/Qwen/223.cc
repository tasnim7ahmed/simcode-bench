#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpAdHocSimulation");

class UdpServer : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("UdpServer")
                            .SetParent<Application>()
                            .SetGroupName("Tutorial")
                            .AddConstructor<UdpServer>();
    return tid;
  }

  UdpServer() : m_socket(nullptr) {}
  virtual ~UdpServer() { if (m_socket) m_socket->Close(); }

private:
  void StartApplication(void)
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
    m_socket->Bind(local);
    m_socket->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, this));
    NS_LOG_INFO("UDP Server listening on port 9");
  }

  void StopApplication(void)
  {
    if (m_socket)
      m_socket->Close();
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

class UdpClient : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("UdpClient")
                            .SetParent<Application>()
                            .SetGroupName("Tutorial")
                            .AddConstructor<UdpClient>()
                            .AddAttribute("RemoteAddress",
                                          "The destination address of UDP packets",
                                          AddressValue(),
                                          MakeAddressAccessor(&UdpClient::m_peerAddress),
                                          MakeAddressChecker())
                            .AddAttribute("Interval",
                                          "Time between consecutive packets",
                                          TimeValue(Seconds(1.0)),
                                          MakeTimeAccessor(&UdpClient::m_interval),
                                          MakeTimeChecker())
                            .AddAttribute("PacketSize",
                                          "Size of the packets",
                                          UintegerValue(512),
                                          MakeUintegerAccessor(&UdpClient::m_packetSize),
                                          MakeUintegerChecker<uint32_t>());
    return tid;
  }

  UdpClient()
      : m_socket(nullptr), m_sendEvent(), m_packetsSent(0)
  {
  }

  virtual ~UdpClient() { if (m_socket) m_socket->Close(); }

private:
  void StartApplication(void)
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Connect(m_peerAddress);
    SendPacket();
  }

  void StopApplication(void)
  {
    if (m_sendEvent.IsRunning())
      Simulator::Cancel(m_sendEvent);
    if (m_socket)
      m_socket->Close();
  }

  void SendPacket(void)
  {
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->Send(packet);
    NS_LOG_INFO("Sent packet number " << ++m_packetsSent << " of size " << m_packetSize);
    m_sendEvent = Simulator::Schedule(m_interval, &UdpClient::SendPacket, this);
  }

  Ptr<Socket> m_socket;
  Address m_peerAddress;
  Time m_interval;
  uint32_t m_packetSize;
  EventId m_sendEvent;
  uint32_t m_packetsSent;
};

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  // Enable logging
  LogComponentEnable("UdpAdHocSimulation", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer nodes;
  nodes.Create(2);

  // Setup Wi-Fi in ad-hoc mode
  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");
  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(channel.Create());
  WifiHelper wifi;
  wifi.SetRemoteStationManager("ns3::ArfRateControl");
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  // Create and install server and client applications
  UdpServerHelper server;
  ApplicationContainer serverApp = server.Install(nodes.Get(1));
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(10.0));

  UdpClientHelper client;
  client.SetAttribute("RemoteAddress", AddressValue(InetSocketAddress(interfaces.GetAddress(1), 9)));
  client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApp = client.Install(nodes.Get(0));
  clientApp.Start(Seconds(1.0));
  clientApp.Stop(Seconds(10.0));

  // Set up global routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Run simulation
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}