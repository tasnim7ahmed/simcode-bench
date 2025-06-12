#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdHocWifiFixedRss");

class AdHocWifiFixedRss : public Object
{
public:
  static TypeId GetTypeId(void);
  AdHocWifiFixedRss();
  virtual ~AdHocWifiFixedRss();

  void Setup(int argc, char *argv[]);
  void RunSimulation();
  void SendPacket(Ptr<Socket> socket);

private:
  void ConfigurePhy(Ptr<WifiPhy> phy);
  void EnableTracing();
  void InstallApplications();

  uint32_t m_packetSize;
  double m_rss;
  double m_rxThreshold;
  bool m_verbose;
  bool m_pcap;
};

TypeId
AdHocWifiFixedRss::GetTypeId(void)
{
  static TypeId tid = TypeId("AdHocWifiFixedRss")
    .SetParent<Object>()
    .AddConstructor<AdHocWifiFixedRss>()
  ;
  return tid;
}

AdHocWifiFixedRss::AdHocWifiFixedRss()
  : m_packetSize(1000),
    m_rss(-60.0),
    m_rxThreshold(-90.0),
    m_verbose(true),
    m_pcap(false)
{
}

AdHocWifiFixedRss::~AdHocWifiFixedRss()
{
}

void
AdHocWifiFixedRss::Setup(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.AddValue("packetSize", "Size of the packet in bytes.", m_packetSize);
  cmd.AddValue("rss", "Received Signal Strength (dBm)", m_rss);
  cmd.AddValue("rxThreshold", "Minimum RSS to receive packet (dBm)", m_rxThreshold);
  cmd.AddValue("verbose", "Enable verbose logging", m_verbose);
  cmd.AddValue("pcap", "Enable pcap tracing", m_pcap);
  cmd.Parse(argc, argv);

  if (!m_verbose)
    {
      LogComponentDisable("AdHocWifiFixedRss", LOG_LEVEL_INFO);
    }
}

void
AdHocWifiFixedRss::RunSimulation()
{
  NodeContainer nodes;
  nodes.Create(2);

  WifiMacHelper wifiMac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy;
  wifiPhy.Set("TxPowerStart", DoubleValue(m_rss));
  wifiPhy.Set("TxPowerEnd", DoubleValue(m_rss));
  wifiPhy.Set("RxGain", DoubleValue(0));
  wifiPhy.Set("CcaEdThreshold", DoubleValue(m_rxThreshold));

  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("DsssRate11Mbps"),
                               "ControlMode", StringValue("DsssRate11Mbps"));

  wifiMac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

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

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  if (m_pcap)
    {
      wifiPhy.EnablePcapAll("adhoc-wifi-fixed-rss");
    }

  Simulator::Schedule(Seconds(1.0), &AdHocWifiFixedRss::InstallApplications, this);
  Simulator::Stop(Seconds(2.0));
  Simulator::Run();
  Simulator::Destroy();
}

void
AdHocWifiFixedRss::InstallApplications()
{
  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  Ptr<Socket> senderSocket = Socket::CreateSocket(GetNode(0), tid);
  InetSocketAddress dest(interfaces.GetAddress(1), 8080);
  senderSocket->Connect(dest);

  Simulator::Schedule(Seconds(0.1), &AdHocWifiFixedRss::SendPacket, this, senderSocket);
}

void
AdHocWifiFixedRss::SendPacket(Ptr<Socket> socket)
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  socket->Send(packet);
  NS_LOG_INFO("Sent packet of size " << m_packetSize << " bytes from Node 0 to Node 1.");
}

int
main(int argc, char *argv[])
{
  AdHocWifiFixedRss app;
  app.Setup(argc, argv);
  app.RunSimulation();
  return 0;
}