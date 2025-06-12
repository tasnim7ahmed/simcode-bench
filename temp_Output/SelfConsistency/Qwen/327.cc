#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/ipv4-list-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetOlsrExample");

class UdpPacketSender : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("UdpPacketSender")
                            .SetParent<Application>()
                            .AddConstructor<UdpPacketSender>();
    return tid;
  }

  UdpPacketSender();
  virtual ~UdpPacketSender();

protected:
  void StartApplication(void);
  void StopApplication(void);

private:
  void SendPacket(void);
  InetSocketAddress m_destination;
  uint16_t m_port;
  Ptr<Socket> m_socket;
};

UdpPacketSender::UdpPacketSender()
    : m_port(9), m_socket(0)
{
}

UdpPacketSender::~UdpPacketSender()
{
  m_socket = 0;
}

void UdpPacketSender::StartApplication(void)
{
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  m_socket->Connect(m_destination);

  Simulator::Schedule(Seconds(1.0), &UdpPacketSender::SendPacket, this);
}

void UdpPacketSender::StopApplication(void)
{
  if (m_socket)
  {
    m_socket->Close();
    m_socket = nullptr;
  }
}

void UdpPacketSender::SendPacket(void)
{
  Ptr<Packet> packet = Create<Packet>(1024); // 1024-byte packet
  m_socket->Send(packet);

  NS_LOG_UNCOND("Packet sent at time " << Simulator::Now().As(Time::S));

  // Schedule next packet after 2 seconds
  Simulator::Schedule(Seconds(2.0), &UdpPacketSender::SendPacket, this);
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  LogComponentEnable("ManetOlsrExample", LOG_LEVEL_INFO);
  LogComponentEnable("UdpPacketSender", LOG_LEVEL_INFO);

  double totalTime = 10.0;
  uint32_t numNodes = 6;

  NodeContainer nodes;
  nodes.Create(numNodes);

  // Mobility configuration
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(20.0),
                                "DeltaY", DoubleValue(20.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                            "Speed", StringValue("ns3::UniformRandomVariable[Min=5.0|Max=15.0]"),
                            "Pause", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                            "PositionAllocator", PointerValue(nullptr));
  mobility.Install(nodes);

  // Wifi configuration
  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper wifiPhy;
  wifiPhy.SetChannel(wifiChannel.Create());
  WifiHelper wifi;
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");

  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  // Internet stack with OLSR routing
  OlsrHelper olsr;
  Ipv4ListRoutingHelper listRH;
  listRH.Add(olsr, 10);

  InternetStackHelper internet;
  internet.SetRoutingHelper(listRH);
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Set up UDP communication from node 0 to node 5
  uint16_t port = 9;
  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(numNodes - 1)); // node 5
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(totalTime));

  Ptr<UdpPacketSender> app = CreateObject<UdpPacketSender>();
  app->m_destination = InetSocketAddress(interfaces.GetAddress(numNodes - 1), port);
  nodes.Get(0)->AddApplication(app);
  app->SetStartTime(Seconds(1.0));
  app->SetStopTime(Seconds(totalTime));

  // Enable ASCII tracing
  AsciiTraceHelper ascii;
  wifiPhy.EnableAsciiAll(ascii.CreateFileStream("manet-olsr-example.tr"));

  // Enable PCAP tracing
  wifiPhy.EnablePcapAll("manet-olsr-example");

  Simulator::Stop(Seconds(totalTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}