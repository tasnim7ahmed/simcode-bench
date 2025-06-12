#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CustomUdpAdHocSimulation");

class UdpServer : public Application
{
public:
  static TypeId GetTypeId()
  {
    static TypeId tid = TypeId("UdpServer")
      .SetParent<Application>()
      .AddConstructor<UdpServer>();
    return tid;
  }

  UdpServer() {}
  virtual ~UdpServer() {}

private:
  virtual void StartApplication()
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
    m_socket->Bind(local);
    m_socket->SetRecvCallback(MakeCallback(&UdpServer::ReceivePacket, this));
    NS_LOG_INFO("UDP Server listening on port 9");
  }

  virtual void StopApplication()
  {
    if (m_socket)
    {
      m_socket->Close();
      m_socket = nullptr;
    }
  }

  void ReceivePacket(Ptr<Socket> socket)
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
  static TypeId GetTypeId()
  {
    static TypeId tid = TypeId("UdpClient")
      .SetParent<Application>()
      .AddConstructor<UdpClient>()
      .AddAttribute("RemoteAddress", "The destination Address of the outbound packets",
                    AddressValue(),
                    MakeAddressAccessor(&UdpClient::m_peerAddress),
                    MakeAddressChecker())
      .AddAttribute("RemotePort", "The destination port of the outbound packets",
                    UintegerValue(9),
                    MakeUintegerAccessor(&UdpClient::m_peerPort),
                    MakeUintegerChecker<uint16_t>())
      .AddAttribute("Interval", "Time between two consecutive packets",
                    TimeValue(Seconds(1.0)),
                    MakeTimeAccessor(&UdpClient::m_interval),
                    MakeTimeChecker())
      .AddAttribute("PacketSize", "Size of the packet to send in bytes",
                    UintegerValue(512),
                    MakeUintegerAccessor(&UdpClient::m_packetSize),
                    MakeUintegerChecker<uint32_t>());
    return tid;
  }

  UdpClient() {}
  virtual ~UdpClient() {}

private:
  virtual void StartApplication()
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    m_socket->Connect(InetSocketAddress(m_peerAddress.GetIpv4(), m_peerPort));
    SendPacket();
  }

  virtual void StopApplication()
  {
    if (m_socket)
    {
      m_socket->Close();
      m_socket = nullptr;
    }
  }

  void SendPacket()
  {
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->Send(packet);
    NS_LOG_INFO("Sent packet of size " << m_packetSize << " to server");
    Simulator::Schedule(m_interval, &UdpClient::SendPacket, this);
  }

  Ptr<Socket> m_socket;
  Address m_peerAddress;
  uint16_t m_peerPort;
  Time m_interval;
  uint32_t m_packetSize;
};

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  WifiMacHelper wifiMac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");

  wifiMac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  ApplicationContainer serverApp;
  UdpServerHelper server;
  serverApp.Add(server.Install(nodes.Get(1)));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  ApplicationContainer clientApp;
  UdpClientHelper client;
  client.SetAttribute("RemoteAddress", AddressValue(interfaces.GetAddress(1)));
  client.SetAttribute("RemotePort", UintegerValue(9));
  client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client.SetAttribute("PacketSize", UintegerValue(512));
  clientApp.Add(client.Install(nodes.Get(0)));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}