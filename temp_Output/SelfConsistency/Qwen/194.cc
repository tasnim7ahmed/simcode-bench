#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiTwoAPsMultipleSTAs");

class StationApplication : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("StationApplication")
                            .SetParent<Application>()
                            .SetGroupName("Tutorial")
                            .AddConstructor<StationApplication>();
    return tid;
  }

  StationApplication();
  virtual ~StationApplication();

  void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, DataRate dataRate);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  void SendPacket(void);

  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_packetSize;
  DataRate m_dataRate;
  EventId m_sendEvent;
  bool m_isRunning;
  uint64_t m_totalData;
};

StationApplication::StationApplication()
    : m_socket(0),
      m_peer(),
      m_packetSize(0),
      m_dataRate(),
      m_isRunning(false),
      m_totalData(0)
{
}

StationApplication::~StationApplication()
{
  m_socket = nullptr;
}

void StationApplication::Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_dataRate = dataRate;
}

void StationApplication::StartApplication()
{
  m_isRunning = true;
  m_totalData = 0;
  SendPacket();
}

void StationApplication::StopApplication()
{
  m_isRunning = false;
  if (m_sendEvent.IsRunning())
  {
    Simulator::Cancel(m_sendEvent);
  }
  NS_LOG_UNCOND("Total data sent: " << m_totalData << " bytes");
}

void StationApplication::SendPacket()
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_totalData += m_packetSize;
  m_socket->SendTo(packet, 0, m_peer);

  Time interval = Seconds((double)m_packetSize * 8 / (double)m_dataRate.GetBitRate());
  m_sendEvent = Simulator::Schedule(interval, &StationApplication::SendPacket, this);
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  // Enable logging
  LogComponentEnable("WiFiTwoAPsMultipleSTAs", LOG_LEVEL_INFO);

  // Create AP and STA nodes
  NodeContainer apNodes;
  apNodes.Create(2);

  NodeContainer staNodes1;
  staNodes1.Create(3); // Group 1 connected to AP 0

  NodeContainer staNodes2;
  staNodes2.Create(3); // Group 2 connected to AP 1

  NodeContainer allNodes;
  allNodes.Add(apNodes);
  allNodes.Add(staNodes1);
  allNodes.Add(staNodes2);

  // Set up mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(5.0),
                                "GridWidth", UintegerValue(5),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(allNodes);

  // Install Wi-Fi
  WifiMacHelper wifiMac;
  WifiHelper wifiHelper;
  wifiHelper.SetStandard(WIFI_STANDARD_80211n_5GHZ);
  wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));

  Ssid ssid1 = Ssid("network-AP1");
  Ssid ssid2 = Ssid("network-AP2");

  // APs
  wifiMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid1),
                  "BeaconInterval", TimeValue(Seconds(2.5)));
  NetDeviceContainer apDevices1 = wifiHelper.Install(wifiMac, apNodes.Get(0));

  wifiMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid2),
                  "BeaconInterval", TimeValue(Seconds(2.5)));
  NetDeviceContainer apDevices2 = wifiHelper.Install(wifiMac, apNodes.Get(1));

  // STAs for AP1
  wifiMac.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(ssid1),
                  "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices1 = wifiHelper.Install(wifiMac, staNodes1);

  // STAs for AP2
  wifiMac.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(ssid2),
                  "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices2 = wifiHelper.Install(wifiMac, staNodes2);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install(allNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("192.168.1.0", "255.255.255.0");

  Ipv4InterfaceContainer apInterfaces1 = ipv4.Assign(apDevices1);
  Ipv4InterfaceContainer apInterfaces2 = ipv4.Assign(apDevices2);
  ipv4.Assign(staDevices1);
  ipv4.Assign(staDevices2);

  // Set up UDP server on AP0
  uint16_t port = 9;
  UdpServerHelper server(port);
  ApplicationContainer serverApp = server.Install(apNodes.Get(0));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  // Set up station applications on each STA
  DataRate dataRate("10Mbps");
  uint32_t packetSize = 1024;

  for (uint32_t i = 0; i < staNodes1.GetN(); ++i)
  {
    Ptr<Node> node = staNodes1.Get(i);
    Ptr<Socket> socket = Socket::CreateSocket(node, UdpSocketFactory::GetTypeId());
    InetSocketAddress remote = InetSocketAddress(apInterfaces1.GetAddress(0), port);
    socket->Connect(remote);

    Ptr<StationApplication> app = CreateObject<StationApplication>();
    app->Setup(socket, remote, packetSize, dataRate);
    node->AddApplication(app);
    app->SetStartTime(Seconds(2.0));
    app->SetStopTime(Seconds(10.0));
  }

  for (uint32_t i = 0; i < staNodes2.GetN(); ++i)
  {
    Ptr<Node> node = staNodes2.Get(i);
    Ptr<Socket> socket = Socket::CreateSocket(node, UdpSocketFactory::GetTypeId());
    InetSocketAddress remote = InetSocketAddress(apInterfaces2.GetAddress(0), port);
    socket->Connect(remote);

    Ptr<StationApplication> app = CreateObject<StationApplication>();
    app->Setup(socket, remote, packetSize, dataRate);
    node->AddApplication(app);
    app->SetStartTime(Seconds(2.0));
    app->SetStopTime(Seconds(10.0));
  }

  // Tracing
  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("wifi-two-aps.tr");
  wifiHelper.EnableAsciiAll(stream);

  wifiHelper.EnablePcapAll("wifi-two-aps");

  // Run simulation
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}