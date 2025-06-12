#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ssid.h"
#include "ns3/config.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/enum.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SpatialReuse80211ax");

class SpatialReuseExperiment
{
public:
  SpatialReuseExperiment();

  void Configure(int argc, char *argv[]);
  void Run();
  void ReceivePacket(Ptr<Socket> socket);

private:
  void SetupNodes();
  void SetupWifi();
  void SetupMobility();
  void SetupApplications();
  void Finalize();

  uint32_t m_packetSize;
  double m_interval;
  double m_distanceApSta1;
  double m_distanceApSta2;
  double m_distanceAp1Ap2;
  double m_txPowerAp1;
  double m_txPowerAp2;
  double m_ccaEdThresholdAp1;
  double m_ccaEdThresholdAp2;
  double m_obssPdThresholdAp1;
  double m_obssPdThresholdAp2;
  bool m_enableObssPd;
  uint32_t m_channelWidth;
  double m_simulationTime;
  std::string m_logFile;

  NodeContainer m_apNodes;
  NodeContainer m_staNodes;
  NetDeviceContainer m_apDevices;
  NetDeviceContainer m_staDevices;
  Ipv4InterfaceContainer m_apInterfaces;
  Ipv4InterfaceContainer m_staInterfaces;

  uint64_t m_bytesReceivedAp1;
  uint64_t m_bytesReceivedAp2;
};

SpatialReuseExperiment::SpatialReuseExperiment()
  : m_packetSize(1024),
    m_interval(0.001), // 1 ms
    m_distanceApSta1(5.0),
    m_distanceApSta2(5.0),
    m_distanceAp1Ap2(20.0),
    m_txPowerAp1(20.0),
    m_txPowerAp2(20.0),
    m_ccaEdThresholdAp1(-82.0),
    m_ccaEdThresholdAp2(-82.0),
    m_obssPdThresholdAp1(-72.0),
    m_obssPdThresholdAp2(-72.0),
    m_enableObssPd(true),
    m_channelWidth(20),
    m_simulationTime(10.0),
    m_logFile("spatial_reuse_log.txt"),
    m_bytesReceivedAp1(0),
    m_bytesReceivedAp2(0)
{
}

void
SpatialReuseExperiment::Configure(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.AddValue("packetSize", "Size of packets in bytes", m_packetSize);
  cmd.AddValue("interval", "Interval between packets (in seconds)", m_interval);
  cmd.AddValue("distanceApSta1", "Distance from AP1 to STA1 (meters)", m_distanceApSta1);
  cmd.AddValue("distanceApSta2", "Distance from AP2 to STA2 (meters)", m_distanceApSta2);
  cmd.AddValue("distanceAp1Ap2", "Distance between AP1 and AP2 (meters)", m_distanceAp1Ap2);
  cmd.AddValue("txPowerAp1", "Transmit power of AP1 (dBm)", m_txPowerAp1);
  cmd.AddValue("txPowerAp2", "Transmit power of AP2 (dBm)", m_txPowerAp2);
  cmd.AddValue("ccaEdThresholdAp1", "CCA-ED threshold for AP1 (dBm)", m_ccaEdThresholdAp1);
  cmd.AddValue("ccaEdThresholdAp2", "CCA-ED threshold for AP2 (dBm)", m_ccaEdThresholdAp2);
  cmd.AddValue("obssPdThresholdAp1", "OBSS-PD threshold for AP1 (dBm)", m_obssPdThresholdAp1);
  cmd.AddValue("obssPdThresholdAp2", "OBSS-PD threshold for AP2 (dBm)", m_obssPdThresholdAp2);
  cmd.AddValue("enableObssPd", "Enable OBSS-PD spatial reuse", m_enableObssPd);
  cmd.AddValue("channelWidth", "Channel width in MHz (20, 40, 80)", m_channelWidth);
  cmd.AddValue("simulationTime", "Duration of simulation in seconds", m_simulationTime);
  cmd.AddValue("logFile", "Output log file name", m_logFile);
  cmd.Parse(argc, argv);
}

void
SpatialReuseExperiment::Run()
{
  SetupNodes();
  SetupWifi();
  SetupMobility();
  SetupApplications();

  NS_LOG_INFO("Starting simulation...");
  Simulator::Run();
  Finalize();
}

void
SpatialReuseExperiment::SetupNodes()
{
  m_apNodes.Create(2);
  m_staNodes.Create(2);
}

void
SpatialReuseExperiment::SetupWifi()
{
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211ax);

  YansWifiPhyHelper phy;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  // Set channel width
  Config::SetDefault("ns3::WifiRemoteStationManager::ChannelWidth", UintegerValue(m_channelWidth));

  // Enable or disable OBSS-PD
  Config::SetDefault("ns3::HeConfiguration::EnableObssPd", BooleanValue(m_enableObssPd));

  // OBSS-PD thresholds per BSS
  Config::SetDefault("ns3::WifiPhy::ObssPdLevel", DoubleValue(m_obssPdThresholdAp1));
  Config::SetDefault("ns3::WifiPhy::ObssPdLevel", DoubleValue(m_obssPdThresholdAp2));

  // CCA-ED Thresholds
  Config::SetDefault("ns3::WifiPhy::CcaEdThreshold", DoubleValue(m_ccaEdThresholdAp1));
  Config::SetDefault("ns3::WifiPhy::CcaEdThreshold", DoubleValue(m_ccaEdThresholdAp2));

  WifiMacHelper mac;
  Ssid ssid1 = Ssid("BSS-AP1");
  Ssid ssid2 = Ssid("BSS-AP2");

  // APs
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid1),
              "BeaconInterval", TimeValue(Seconds(2.0)));
  m_apDevices.Add(wifi.Install(phy, mac, m_apNodes.Get(0)));

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid2),
              "BeaconInterval", TimeValue(Seconds(2.0)));
  m_apDevices.Add(wifi.Install(phy, mac, m_apNodes.Get(1)));

  // STAs
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid1));
  m_staDevices.Add(wifi.Install(phy, mac, m_staNodes.Get(0)));

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid2));
  m_staDevices.Add(wifi.Install(phy, mac, m_staNodes.Get(1)));

  // Set transmit power
  Ptr<WifiPhy> phyAp1 = m_apDevices.Get(0)->GetPhy();
  if (phyAp1)
    {
      phyAp1->SetTxPowerStart(m_txPowerAp1);
      phyAp1->SetTxPowerEnd(m_txPowerAp1);
    }

  Ptr<WifiPhy> phyAp2 = m_apDevices.Get(1)->GetPhy();
  if (phyAp2)
    {
      phyAp2->SetTxPowerStart(m_txPowerAp2);
      phyAp2->SetTxPowerEnd(m_txPowerAp2);
    }
}

void
SpatialReuseExperiment::SetupMobility()
{
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(m_distanceAp1Ap2),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(m_apNodes);

  // Place STA1 near AP1 and STA2 near AP2
  Ptr<ListPositionAllocator> staPositionAlloc = CreateObject<ListPositionAllocator>();
  staPositionAlloc->Add(Vector(m_distanceApSta1, 0.0, 0.0)); // STA1 near AP1
  staPositionAlloc->Add(Vector(m_distanceAp1Ap2 - m_distanceApSta2, 0.0, 0.0)); // STA2 near AP2

  mobility.SetPositionAllocator(staPositionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(m_staNodes);
}

void
SpatialReuseExperiment::SetupApplications()
{
  InternetStackHelper internet;
  internet.Install(m_apNodes);
  internet.Install(m_staNodes);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  m_apInterfaces = address.Assign(m_apDevices);
  m_staInterfaces = address.Assign(m_staDevices);

  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");

  // Server sockets on APs
  Ptr<Socket> ap1Socket = Socket::CreateSocket(m_apNodes.Get(0), tid);
  InetSocketAddress ap1Local = InetSocketAddress(Ipv4Address::GetAny(), 8080);
  ap1Socket->Bind(ap1Local);
  ap1Socket->SetRecvCallback(MakeCallback(&SpatialReuseExperiment::ReceivePacket, this));

  Ptr<Socket> ap2Socket = Socket::CreateSocket(m_apNodes.Get(1), tid);
  InetSocketAddress ap2Local = InetSocketAddress(Ipv4Address::GetAny(), 8080);
  ap2Socket->Bind(ap2Local);
  ap2Socket->SetRecvCallback(MakeCallback(&SpatialReuseExperiment::ReceivePacket, this));

  // Client traffic from STAs to APs
  Ptr<Socket> sta1Socket = Socket::CreateSocket(m_staNodes.Get(0), tid);
  InetSocketAddress remoteAp1 = InetSocketAddress(m_apInterfaces.GetAddress(0), 8080);
  sta1Socket->Connect(remoteAp1);

  Ptr<Socket> sta2Socket = Socket::CreateSocket(m_staNodes.Get(1), tid);
  InetSocketAddress remoteAp2 = InetSocketAddress(m_apInterfaces.GetAddress(1), 8080);
  sta2Socket->Connect(remoteAp2);

  Simulator::ScheduleNow([this, sta1Socket]() {
    while (true)
      {
        Ptr<Packet> packet = Create<Packet>(m_packetSize);
        sta1Socket->Send(packet);
        Simulator::Schedule(Seconds(m_interval), &SpatialReuseExperiment::Run, this);
        break;
      }
  });

  Simulator::ScheduleNow([this, sta2Socket]() {
    while (true)
      {
        Ptr<Packet> packet = Create<Packet>(m_packetSize);
        sta2Socket->Send(packet);
        Simulator::Schedule(Seconds(m_interval), &SpatialReuseExperiment::Run, this);
        break;
      }
  });
}

void
SpatialReuseExperiment::ReceivePacket(Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet;
  while ((packet = socket->RecvFrom(from)))
    {
      if (socket->GetNode()->GetId() == m_apNodes.Get(0)->GetId())
        {
          m_bytesReceivedAp1 += packet->GetSize();
        }
      else if (socket->GetNode()->GetId() == m_apNodes.Get(1)->GetId())
        {
          m_bytesReceivedAp2 += packet->GetSize();
        }
    }
}

void
SpatialReuseExperiment::Finalize()
{
  double throughputAp1 = (m_bytesReceivedAp1 * 8) / (m_simulationTime * 1000000); // Mbps
  double throughputAp2 = (m_bytesReceivedAp2 * 8) / (m_simulationTime * 1000000); // Mbps

  std::ofstream log(m_logFile.c_str(), std::ios::app);
  log << "Simulation Parameters:\n";
  log << "OBSS-PD Enabled: " << m_enableObssPd << "\n";
  log << "Channel Width: " << m_channelWidth << " MHz\n";
  log << "Throughput AP1: " << throughputAp1 << " Mbps\n";
  log << "Throughput AP2: " << throughputAp2 << " Mbps\n";
  log << "---------------------------\n";
  log.close();

  std::cout << "Throughput AP1: " << throughputAp1 << " Mbps" << std::endl;
  std::cout << "Throughput AP2: " << throughputAp2 << " Mbps" << std::endl;

  Simulator::Destroy();
}

int
main(int argc, char *argv[])
{
  LogComponentEnable("SpatialReuse80211ax", LOG_LEVEL_INFO);
  SpatialReuseExperiment experiment;
  experiment.Configure(argc, argv);
  experiment.Run();
}