#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiBroadcastExample");

class UdpRxLogger : public Application
{
public:
  UdpRxLogger() {}
  virtual ~UdpRxLogger() {}
  
  void Setup(Address address, uint16_t port)
  {
    m_address = address;
    m_port = port;
  }
  
private:
  virtual void StartApplication(void)
  {
    if (!m_socket)
    {
      m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
      InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
      m_socket->Bind(local);
      m_socket->SetRecvCallback(MakeCallback(&UdpRxLogger::HandleRead, this));
    }
  }
  
  virtual void StopApplication(void)
  {
    if (m_socket)
    {
      m_socket->Close();
      m_socket = 0;
    }
  }
  
  void HandleRead(Ptr<Socket> socket)
  {
    while (Ptr<Packet> packet = socket->Recv())
    {
      NS_LOG_UNCOND("Node " << GetNode()->GetId() << " received packet of size " << packet->GetSize() << " at " << Simulator::Now().GetSeconds() << "s");
    }
  }

  Ptr<Socket> m_socket;
  Address m_address;
  uint16_t m_port;
};

int main(int argc, char *argv[])
{
  uint32_t numReceivers = 4; // Number of receiver nodes
  uint32_t packetSize = 1024;
  uint32_t nPackets = 10;
  double interval = 1.0; // seconds
  uint16_t udpPort = 50000;

  CommandLine cmd;
  cmd.AddValue("receivers", "Number of receiver nodes", numReceivers);
  cmd.Parse(argc, argv);

  uint32_t numNodes = numReceivers + 1; // 1 sender + receivers

  NodeContainer nodes;
  nodes.Create(numNodes);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211g);

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid("simple-wifi");

  wifiMac.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(ssid),
                  "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(wifiPhy, wifiMac, NodeContainer(nodes.Get(1), nodes.Get(numNodes-1)));

  wifiMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevice;
  apDevice = wifi.Install(wifiPhy, wifiMac, nodes.Get(0));

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  for (uint32_t i = 0; i < numNodes; ++i)
  {
    positionAlloc->Add(Vector(5.0 * i, 0.0, 0.0));
  }
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  NetDeviceContainer allDevices;
  allDevices.Add(apDevice);
  allDevices.Add(staDevices);
  Ipv4InterfaceContainer interfaces = address.Assign(allDevices);

  // UDP sender (broadcast)
  Ptr<Socket> source = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());
  InetSocketAddress broadcastAddr = InetSocketAddress(Ipv4Address("10.1.1.255"), udpPort);
  source->SetAllowBroadcast(true);
  source->Connect(broadcastAddr);

  // UDP receiver apps on all receiver nodes
  for (uint32_t i = 1; i < numNodes; ++i)
  {
    Ptr<UdpRxLogger> app = CreateObject<UdpRxLogger>();
    app->Setup(Ipv4Address::GetAny(), udpPort);
    nodes.Get(i)->AddApplication(app);
    app->SetStartTime(Seconds(0.0));
    app->SetStopTime(Seconds(nPackets * interval + 5));
  }
  
  // Send broadcast packets from sender node
  for (uint32_t i = 0; i < nPackets; ++i)
  {
    Simulator::Schedule(Seconds(i * interval), [source, packetSize]() {
      Ptr<Packet> packet = Create<Packet>(packetSize);
      source->Send(packet);
      NS_LOG_UNCOND("Sender sent packet at " << Simulator::Now().GetSeconds() << "s");
    });
  }

  Simulator::Stop(Seconds(nPackets * interval + 10));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}