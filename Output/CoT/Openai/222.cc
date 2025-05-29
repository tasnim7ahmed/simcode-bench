#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CustomUdpWifiExample");

// Custom UDP Server Application
class CustomUdpServer : public Application
{
public:
  CustomUdpServer() : m_socket(0) {}
  virtual ~CustomUdpServer() { m_socket = 0; }

  void Setup(Address address, uint16_t port)
  {
    m_local = InetSocketAddress(Ipv4Address::ConvertFrom(address), port);
    m_port = port;
  }

private:
  virtual void StartApplication()
  {
    if(!m_socket)
    {
      m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
      m_socket->Bind(m_local);
    }
    m_socket->SetRecvCallback(MakeCallback(&CustomUdpServer::HandleRead, this));
  }

  virtual void StopApplication()
  {
    if(m_socket)
    {
      m_socket->Close();
    }
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      uint32_t size = packet->GetSize();
      InetSocketAddress addr = InetSocketAddress::ConvertFrom(from);
      NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: Server received " << size << " bytes from " << addr.GetIpv4());
    }
  }

  Ptr<Socket> m_socket;
  Address m_local;
  uint16_t m_port;
};

// Custom UDP Client Application
class CustomUdpClient : public Application
{
public:
  CustomUdpClient() : m_socket(0), m_sendEvent(), m_running(false), m_packetsSent(0) {}
  virtual ~CustomUdpClient() { m_socket = 0; }

  void Setup(Address address, uint16_t port, uint32_t packetSize, uint32_t nPackets, Time interval)
  {
    m_peer = InetSocketAddress(Ipv4Address::ConvertFrom(address), port);
    m_packetSize = packetSize;
    m_nPackets = nPackets;
    m_interval = interval;
  }

private:
  virtual void StartApplication()
  {
    m_running = true;
    m_packetsSent = 0;
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Connect(m_peer);
    SendPacket();
  }

  virtual void StopApplication()
  {
    m_running = false;
    if (m_sendEvent.IsRunning())
      Simulator::Cancel(m_sendEvent);
    if (m_socket)
      m_socket->Close();
  }

  void SendPacket()
  {
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->Send(packet);
    ++m_packetsSent;
    if (m_packetsSent < m_nPackets)
    {
      m_sendEvent = Simulator::Schedule(m_interval, &CustomUdpClient::SendPacket, this);
    }
  }

  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_packetSize;
  uint32_t m_nPackets;
  Time m_interval;
  EventId m_sendEvent;
  bool m_running;
  uint32_t m_packetsSent;
};

int main (int argc, char *argv[])
{
  uint32_t packetSize = 1024;
  uint32_t numPackets = 100;
  double interval = 0.1;
  double simTime = 10.0;
  uint16_t udpPort = 4000;

  // Create nodes: Server, AP, Client
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(1);
  Ptr<Node> clientNode = wifiStaNodes.Get(0);

  NodeContainer wifiApNode;
  wifiApNode.Create(1);
  Ptr<Node> apNode = wifiApNode.Get(0);

  NodeContainer serverNode;
  serverNode.Create(1);
  Ptr<Node> srvNode = serverNode.Get(0);

  // Create Wi-Fi channel and configure physical and MAC layers
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns3-ssid");

  // Client STA
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevice;
  staDevice = wifi.Install(phy, mac, wifiStaNodes);

  // AP MAC
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install(phy, mac, wifiApNode);

  // Add server node with CSMA to AP
  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

  NodeContainer csmaNodes;
  csmaNodes.Add(wifiApNode.Get(0));
  csmaNodes.Add(serverNode.Get(0));

  NetDeviceContainer csmaDevices;
  csmaDevices = csma.Install(csmaNodes);

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
  mobility.Install(wifiApNode);
  mobility.Install(serverNode);

  // Internet stack
  InternetStackHelper stack;
  stack.Install(wifiStaNodes);
  stack.Install(wifiApNode);
  stack.Install(serverNode);

  // Assign IP addresses
  Ipv4AddressHelper address;

  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterface;
  staInterface = address.Assign(staDevice);

  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign(apDevice);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces;
  csmaInterfaces = address.Assign(csmaDevices);

  // Enable static routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Install custom UDP server on server node
  Ptr<CustomUdpServer> serverApp = CreateObject<CustomUdpServer>();
  serverApp->Setup(csmaInterfaces.GetAddress(1), udpPort);
  srvNode->AddApplication(serverApp);
  serverApp->SetStartTime(Seconds(0.0));
  serverApp->SetStopTime(Seconds(simTime));

  // Install custom UDP client on STA node
  Ptr<CustomUdpClient> clientApp = CreateObject<CustomUdpClient>();
  // set numPackets so (numPackets-1)*interval < simTime
  clientApp->Setup(csmaInterfaces.GetAddress(1), udpPort, packetSize, uint32_t(simTime/interval), Seconds(interval));
  clientNode->AddApplication(clientApp);
  clientApp->SetStartTime(Seconds(1.0));
  clientApp->SetStopTime(Seconds(simTime));

  // Enable pcap tracing if desired
  phy.EnablePcap("udp-wifi-client", staDevice.Get(0));
  csma.EnablePcap("udp-wifi-server", csmaDevices.Get(1), true);

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}