#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/stats-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiDistanceExperiment");

class WiFiDistanceExperiment : public Object
{
public:
  static TypeId GetTypeId(void);
  WiFiDistanceExperiment();
  virtual ~WiFiDistanceExperiment();

  void Setup(Ptr<Node> sender, Ptr<Node> receiver, double distance);
  void Start(uint32_t packetSize, uint32_t numPackets, Time interval);
  void Report();

private:
  void OnTx(Ptr<const Packet> packet);
  void OnRx(Ptr<const Packet> packet, const Address &from);

  uint32_t m_packetsTransmitted;
  uint32_t m_packetsReceived;
  DataCollector m_dataCollector;
};

TypeId WiFiDistanceExperiment::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::WiFiDistanceExperiment")
                          .SetParent<Object>()
                          .AddConstructor<WiFiDistanceExperiment>();
  return tid;
}

WiFiDistanceExperiment::WiFiDistanceExperiment()
    : m_packetsTransmitted(0), m_packetsReceived(0)
{
  m_dataCollector.Start(Time::FromDouble(1.0, Time::S));
}

WiFiDistanceExperiment::~WiFiDistanceExperiment()
{
}

void WiFiDistanceExperiment::Setup(Ptr<Node> sender, Ptr<Node> receiver, double distance)
{
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0));      // Sender at origin
  positionAlloc->Add(Vector(distance, 0.0, 0.0)); // Receiver at distance meters away

  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(sender);
  mobility.Install(receiver);

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);

  WifiMacHelper mac;
  mac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices;
  devices = wifi.Install(phy, mac, NodeContainer(sender, receiver));

  InternetStackHelper stack;
  stack.Install(NodeContainer(sender, receiver));

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(receiver);
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(0));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(sender);
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpEchoClient/Tx", MakeCallback(&WiFiDistanceExperiment::OnTx, this));
  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpEchoClient/Rx", MakeCallback(&WiFiDistanceExperiment::OnRx, this));

  m_dataCollector.AddDataSeries("Packets Transmitted", m_packetsTransmitted);
  m_dataCollector.AddDataSeries("Packets Received", m_packetsReceived);
}

void WiFiDistanceExperiment::Start(uint32_t packetSize, uint32_t numPackets, Time interval)
{
  Simulator::ScheduleNow([this, packetSize, numPackets, interval]() {
    for (uint32_t i = 0; i < numPackets; ++i)
    {
      Simulator::Schedule(Simulator::Now() + i * interval, [this, packetSize]() {
        Ptr<Packet> packet = Create<Packet>(packetSize);
        // Transmit logic via application or socket if needed
      });
    }
  });
}

void WiFiDistanceExperiment::Report()
{
  NS_LOG_UNCOND("Total Packets Transmitted: " << m_packetsTransmitted);
  NS_LOG_UNCOND("Total Packets Received: " << m_packetsReceived);
}

void WiFiDistanceExperiment::OnTx(Ptr<const Packet> packet)
{
  m_packetsTransmitted++;
}

void WiFiDistanceExperiment::OnRx(Ptr<const Packet> packet, const Address &from)
{
  m_packetsReceived++;
}

int main(int argc, char *argv[])
{
  double distance = 50.0;
  std::string format = "omnet";
  std::string runId = "default_run";

  CommandLine cmd(__FILE__);
  cmd.AddValue("distance", "Distance between nodes in meters", distance);
  cmd.AddValue("format", "Output format (omnet or sqlite)", format);
  cmd.AddValue("run", "Run identifier", runId);
  cmd.Parse(argc, argv);

  Ptr<WiFiDistanceExperiment> experiment = CreateObject<WiFiDistanceExperiment>();

  Ptr<Node> sender = CreateObject<Node>();
  Ptr<Node> receiver = CreateObject<Node>();

  experiment->Setup(sender, receiver, distance);
  experiment->Start(1024, 5, Seconds(1.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  experiment->Report();

  if (format == "omnet")
  {
    OmnetDataOutput omnetOutput;
    omnetOutput.Output(experiment->GetDataCollector());
  }
  else if (format == "sqlite")
  {
    SqliteDataOutput sqliteOutput;
    sqliteOutput.SetFilePrefix(runId);
    sqliteOutput.Output(experiment->GetDataCollector());
  }

  Simulator::Destroy();
  return 0;
}