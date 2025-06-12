#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CustomUdpWifiSimulation");

class CustomUdpServer : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("CustomUdpServer")
      .SetParent<Application>()
      .AddConstructor<CustomUdpServer>();
    return tid;
  }

  CustomUdpServer() {}
  virtual ~CustomUdpServer() {}

private:
  virtual void StartApplication(void)
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
    m_socket->Bind(local);
    m_socket->SetRecvCallback(MakeCallback(&CustomUdpServer::HandleRead, this));
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
      NS_LOG_UNCOND("Server received packet of size " << packet->GetSize() << " from " << from);
    }
  }

  Ptr<Socket> m_socket;
};

class CustomUdpClient : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("CustomUdpClient")
      .SetParent<Application>()
      .AddConstructor<CustomUdpClient>()
      .AddAttribute("RemoteAddress", "The destination Address of the outbound packets.",
                    AddressValue(),
                    MakeAddressAccessor(&CustomUdpClient::m_peerAddress),
                    MakeAddressChecker())
      .AddAttribute("Interval", "The time between consecutive packets",
                    TimeValue(Seconds(1.0)),
                    MakeTimeAccessor(&CustomUdpClient::m_interval),
                    MakeTimeChecker())
      .AddAttribute("PacketSize", "Size of the sent packets (bytes)",
                    UintegerValue(512),
                    MakeUintegerAccessor(&CustomUdpClient::m_pktSize),
                    MakeUintegerChecker<uint32_t>());
    return tid;
  }

  CustomUdpClient() {}
  virtual ~CustomUdpClient() {}

private:
  virtual void StartApplication(void)
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_sendEvent = Simulator::ScheduleNow(&CustomUdpClient::SendPacket, this);
  }

  virtual void StopApplication(void)
  {
    Simulator::Cancel(m_sendEvent);
    if (m_socket)
    {
      m_socket->Close();
      m_socket = nullptr;
    }
  }

  void SendPacket(void)
  {
    Ptr<Packet> packet = Create<Packet>(m_pktSize);
    m_socket->SendTo(packet, 0, m_peerAddress);
    NS_LOG_UNCOND("Client sent packet to " << m_peerAddress);
    m_sendEvent = Simulator::Schedule(m_interval, &CustomUdpClient::SendPacket, this);
  }

  Address m_peerAddress;
  Time m_interval;
  uint32_t m_pktSize;
  EventId m_sendEvent;
  Ptr<Socket> m_socket;
};

int main(int argc, char *argv[])
{
  LogComponentEnable("CustomUdpWifiSimulation", LOG_LEVEL_INFO);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(1);
  NodeContainer apNode;
  apNode.Create(1);

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
  apDevice = wifi.Install(phy, mac, apNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(wifiStaNodes);
  mobility.Install(apNode);

  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign(apDevice);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Server application on AP node
  CustomUdpServerHelper serverApp;
  ApplicationContainer serverApps = serverApp.Install(apNode.Get(0));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(10.0));

  // Client application on STA node
  CustomUdpClientHelper clientApp;
  clientApp.SetAttribute("RemoteAddress", AddressValue(InetSocketAddress(apInterface.GetAddress(0), 9)));
  clientApp.SetAttribute("Interval", TimeValue(Seconds(2.0)));
  clientApp.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = clientApp.Install(wifiStaNodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}