#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/olsr-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/dsr-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include <vector>
#include <cstdlib>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetUdpAodvExample");

class UdpSenderApplication : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("UdpSenderApplication")
                            .SetParent<Application>()
                            .SetGroupName("Tutorial")
                            .AddConstructor<UdpSenderApplication>();
    return tid;
  }

  UdpSenderApplication();
  virtual ~UdpSenderApplication();

  void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, DataRate dataRate);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);
  void SendPacket(void);
  void ScheduleTx(void);

  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_packetSize;
  DataRate m_dataRate;
  EventId m_sendEvent;
  bool m_isRunning;
};

UdpSenderApplication::UdpSenderApplication()
    : m_socket(0),
      m_peer(),
      m_packetSize(0),
      m_dataRate("1kbps"),
      m_sendEvent(),
      m_isRunning(false)
{
}

UdpSenderApplication::~UdpSenderApplication()
{
  m_socket = nullptr;
}

void UdpSenderApplication::Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_dataRate = dataRate;
}

void UdpSenderApplication::StartApplication(void)
{
  m_isRunning = true;
  m_socket->Bind();
  m_socket->Connect(m_peer);
  SendPacket();
}

void UdpSenderApplication::StopApplication(void)
{
  m_isRunning = false;

  if (m_sendEvent.IsRunning())
  {
    Simulator::Cancel(m_sendEvent);
  }

  if (m_socket)
  {
    m_socket->Close();
  }
}

void UdpSenderApplication::SendPacket(void)
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);

  ScheduleTx();
}

void UdpSenderApplication::ScheduleTx(void)
{
  if (m_isRunning)
  {
    Time tNext(Seconds(m_packetSize * 8.0 / m_dataRate.GetBitRate()));
    m_sendEvent = Simulator::Schedule(tNext, &UdpSenderApplication::SendPacket, this);
  }
}

static Ipv4Address
GetRandomNeighborIpv4Address(NodeContainer nodes, uint32_t currentIdx)
{
  uint32_t totalNodes = nodes.GetN();
  std::vector<Ipv4Address> addresses;

  for (uint32_t i = 0; i < totalNodes; ++i)
  {
    if (i != currentIdx)
    {
      Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
      Ipv4InterfaceAddress iaddr = ipv4->GetInterface(1).GetAddress(0);
      addresses.push_back(iaddr.GetLocal());
    }
  }

  if (!addresses.empty())
  {
    return addresses[rand() % addresses.size()];
  }

  return Ipv4Address::GetZero();
}

int main(int argc, char *argv[])
{
  LogComponentEnable("ManetUdpAodvExample", LOG_LEVEL_INFO);
  LogComponentEnable("UdpSenderApplication", LOG_LEVEL_INFO);

  uint32_t numNodes = 10;
  double radius = 150.0;
  double totalTime = 20.0;
  uint32_t port = 9;
  uint32_t packetSize = 512;
  DataRate dataRate("1kbps");

  CommandLine cmd(__FILE__);
  cmd.AddValue("numNodes", "Number of nodes in the MANET", numNodes);
  cmd.AddValue("radius", "Radius of the simulation area", radius);
  cmd.AddValue("totalTime", "Total simulation time in seconds", totalTime);
  cmd.Parse(argc, argv);

  // Seed randomness
  srand(time(0));

  // Create nodes
  NodeContainer nodes;
  nodes.Create(numNodes);

  // Setup mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(radius) + "]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(radius) + "]"));
  mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                            "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                            "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.2]"));
  mobility.Install(nodes);

  // Setup wifi
  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");
  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());
  WifiHelper wifi;
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  // Install internet stack with AODV
  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingHelper(aodv);
  internet.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Setup applications
  ApplicationContainer apps;

  for (uint32_t i = 0; i < numNodes; ++i)
  {
    Ipv4Address destAddr = GetRandomNeighborIpv4Address(nodes, i);

    if (destAddr == Ipv4Address::GetZero())
    {
      NS_LOG_WARN("Node " << i << " could not find a neighbor to send packets to.");
      continue;
    }

    NS_LOG_INFO("Node " << i << " sending packets to " << destAddr);

    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    Ptr<Socket> udpSocket = Socket::CreateSocket(nodes.Get(i), tid);

    Ptr<UdpSenderApplication> app = CreateObject<UdpSenderApplication>();
    app->Setup(udpSocket, InetSocketAddress(destAddr, port), packetSize, dataRate);
    nodes.Get(i)->AddApplication(app);
    app->SetStartTime(Seconds(1.0));
    app->SetStopTime(Seconds(totalTime));
  }

  // Enable PCAP tracing
  wifiPhy.EnablePcapAll("manet-udp-aodv");

  Simulator::Stop(Seconds(totalTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}