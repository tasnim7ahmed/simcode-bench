#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleWifiInfrastructure");

class SimpleWifiApp : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("SimpleWifiApp")
                            .SetParent<Application>()
                            .AddConstructor<SimpleWifiApp>()
                            .AddAttribute("PacketSize", "Size of packets sent.",
                                          UintegerValue(1024),
                                          MakeUintegerAccessor(&SimpleWifiApp::m_packetSize),
                                          MakeUintegerChecker<uint32_t>())
                            .AddAttribute("NumPackets", "Number of packets to send.",
                                          UintegerValue(50),
                                          MakeUintegerAccessor(&SimpleWifiApp::m_numPackets),
                                          MakeUintegerChecker<uint32_t>())
                            .AddAttribute("Interval", "Interval between packets.",
                                          TimeValue(Seconds(1.0)),
                                          MakeTimeAccessor(&SimpleWifiApp::m_interval),
                                          MakeTimeChecker())
                            .AddAttribute("RemoteAddress", "Destination address for sending packets.",
                                          AddressValue(),
                                          MakeAddressAccessor(&SimpleWifiApp::m_peer),
                                          MakeAddressChecker());
    return tid;
  }

  SimpleWifiApp();
  virtual ~SimpleWifiApp();

protected:
  void StartApplication(void);
  void StopApplication(void);

private:
  void SendPacket(void);

  uint32_t m_packetSize;
  uint32_t m_numPackets;
  Time m_interval;
  Address m_peer;
  uint32_t m_packetsSent;
  EventId m_sendEvent;
  Ptr<Socket> m_socket;
};

SimpleWifiApp::SimpleWifiApp()
    : m_packetSize(1024), m_numPackets(50), m_interval(Seconds(1.0)), m_packetsSent(0)
{
}

void SimpleWifiApp::StartApplication(void)
{
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  m_socket->Connect(m_peer);
  SendPacket();
}

void SimpleWifiApp::StopApplication(void)
{
  if (m_sendEvent.IsRunning())
  {
    Simulator::Cancel(m_sendEvent);
  }

  if (m_socket)
  {
    m_socket->Close();
  }
}

void SimpleWifiApp::SendPacket(void)
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);

  m_packetsSent++;
  NS_LOG_INFO("Sent packet number " << m_packetsSent << " at " << Now().As(Time::S));

  if (m_packetsSent < m_numPackets)
  {
    m_sendEvent = Simulator::Schedule(m_interval, &SimpleWifiApp::SendPacket, this);
  }
}

int main(int argc, char *argv[])
{
  std::string phyMode("DsssRate1Mbps");
  double rss = -80;        // dBm
  uint32_t packetSize = 1024;
  uint32_t numPackets = 50;
  double interval = 1.0;   // seconds
  bool verbose = false;
  std::string pcapFile = "simple-wifi.pcap";

  CommandLine cmd(__FILE__);
  cmd.AddValue("rss", "Received Signal Strength (dBm)", rss);
  cmd.AddValue("packetSize", "Size of packets in bytes", packetSize);
  cmd.AddValue("numPackets", "Number of packets to send", numPackets);
  cmd.AddValue("interval", "Interval between packets in seconds", interval);
  cmd.AddValue("verbose", "Enable verbose logging output", verbose);
  cmd.Parse(argc, argv);

  if (verbose)
  {
    LogComponentEnable("SimpleWifiInfrastructure", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  }

  NodeContainer wifiStaNode;
  wifiStaNode.Create(1);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");

  WifiMacHelper mac;
  Ssid ssid = Ssid("simple-wifi-network");

  // Station
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid));
  NetDeviceContainer staDevice;
  staDevice = wifi.Install(phy, mac, wifiStaNode);

  // Access Point
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install(phy, mac, wifiApNode);

  // Mobility model: fixed position
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNode);
  mobility.Install(wifiApNode);

  // RSS fixed loss model
  Ptr<FixedRssLossModel> lossModel = CreateObject<FixedRssLossModel>();
  lossModel->SetRss(rss);
  phy.GetPhy(0)->SetAdditionalLossModel(lossModel);

  InternetStackHelper stack;
  stack.Install(wifiStaNode);
  stack.Install(wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterface = address.Assign(staDevice);
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

  // Enable PCAP tracing
  phy.EnablePcap(pcapFile, apDevice.Get(0));

  // Install application on station to send to AP
  Ptr<SimpleWifiApp> app = CreateObject<SimpleWifiApp>();
  app->SetAttribute("PacketSize", UintegerValue(packetSize));
  app->SetAttribute("NumPackets", UintegerValue(numPackets));
  app->SetAttribute("Interval", TimeValue(Seconds(interval)));
  app->SetAttribute("RemoteAddress", AddressValue(InetSocketAddress(apInterface.GetAddress(0), 9)));

  wifiStaNode.Get(0)->AddApplication(app);
  app->SetStartTime(Seconds(1.0));
  app->SetStopTime(Seconds(1.0 + interval * numPackets));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}