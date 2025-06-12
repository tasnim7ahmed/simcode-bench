#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CustomUdpWifiSimulation");

class UdpServer : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("UdpServer")
      .SetParent<Application>()
      .SetGroupName("Applications")
      .AddConstructor<UdpServer>();
    return tid;
  }

  UdpServer() {}
  virtual ~UdpServer() {}

protected:
  void DoInitialize() override
  {
    Application::DoInitialize();
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
    m_socket->Bind(local);
    m_socket->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, this));
  }

  void DoDispose() override
  {
    m_socket->Close();
    Application::DoDispose();
  }

private:
  void StartApplication() override {}
  void StopApplication() override {}

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
      .SetGroupName("Applications")
      .AddConstructor<UdpClient>()
      .AddAttribute("RemoteAddress", "The destination Address of the outbound packets",
                    AddressValue(),
                    MakeAddressAccessor(&UdpClient::m_peerAddress),
                    MakeAddressChecker())
      .AddAttribute("Interval", "The time between consecutive packet sends",
                    TimeValue(Seconds(1.0)),
                    MakeTimeAccessor(&UdpClient::m_interval),
                    MakeTimeChecker())
      .AddAttribute("PacketSize", "Size of the sent packets (including UDP header)",
                    UintegerValue(1024),
                    MakeUintegerAccessor(&UdpClient::m_pktSize),
                    MakeUintegerChecker<uint32_t>());
    return tid;
  }

  UdpClient() {}
  virtual ~UdpClient() {}

protected:
  void DoInitialize() override
  {
    Application::DoInitialize();
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  }

  void DoDispose() override
  {
    m_socket->Close();
    Application::DoDispose();
  }

private:
  void StartApplication() override
  {
    m_sendEvent = Simulator::Schedule(Seconds(0.0), &UdpClient::SendPacket, this);
  }

  void StopApplication() override
  {
    Simulator::Cancel(m_sendEvent);
  }

  void SendPacket()
  {
    Ptr<Packet> packet = Create<Packet>(m_pktSize);
    m_socket->SendTo(packet, 0, m_peerAddress);

    NS_LOG_INFO("Sent packet of size " << m_pktSize << " to " << m_peerAddress);

    m_sendEvent = Simulator::Schedule(m_interval, &UdpClient::SendPacket, this);
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
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(1);

  NodeContainer apNode;
  apNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, wifiStaNodes);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevice;
  apDevice = wifi.Install(phy, mac, apNode);

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
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

  // Install server on AP node
  UdpServerHelper serverHelper;
  ApplicationContainer serverApp = serverHelper.Install(apNode.Get(0));
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(10.0));

  // Install client on STA node
  UdpClientHelper clientHelper;
  clientHelper.SetAttribute("RemoteAddress", AddressValue(InetSocketAddress(apInterface.GetAddress(0), 9)));
  clientHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  clientHelper.SetAttribute("PacketSize", UintegerValue(512));

  ApplicationContainer clientApp = clientHelper.Install(wifiStaNodes.Get(0));
  clientApp.Start(Seconds(1.0));
  clientApp.Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}