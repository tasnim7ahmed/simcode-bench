#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/stats-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiDistancePerformanceExperiment");

class ExperimentHelper {
public:
  ExperimentHelper(double distance, std::string formatType, uint32_t runId)
    : m_distance(distance),
      m_formatType(formatType),
      m_runId(runId),
      m_packetsTransmitted(0),
      m_packetsReceived(0),
      m_totalDelay(Seconds(0)) {}

  void Run();
  void TransmitCallback(Ptr<const Packet> packet);
  void ReceiveCallback(Ptr<const Packet> packet, const Address& from);

private:
  double m_distance;
  std::string m_formatType;
  uint32_t m_runId;
  uint32_t m_packetsTransmitted;
  uint32_t m_packetsReceived;
  Time m_totalDelay;
  DataCollector m_dataCollector;
};

void ExperimentHelper::TransmitCallback(Ptr<const Packet> packet) {
  m_packetsTransmitted++;
  m_dataCollector.DescribeDouble("PacketsTransmitted", DoubleValue(m_packetsTransmitted));
}

void ExperimentHelper::ReceiveCallback(Ptr<const Packet> packet, const Address& from) {
  m_packetsReceived++;
  m_dataCollector.DescribeDouble("PacketsReceived", DoubleValue(m_packetsReceived));
  m_totalDelay += Simulator::Now();
}

void ExperimentHelper::Run() {
  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("1500"));

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  channel.AddPropagationLoss("ns3::LogDistancePropagationLossModel");
  channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");

  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
  mac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(m_distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(20));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  clientApps.Get(0)->TraceConnectWithoutContext("Tx", MakeCallback(&ExperimentHelper::TransmitCallback, this));
  serverApps.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&ExperimentHelper::ReceiveCallback, this));

  m_dataCollector.Start(Time(Seconds(0)));
  m_dataCollector.End(Time(Seconds(10)));

  m_dataCollector.DescribeRun(m_runId, "wifi-distance-experiment", "distance=" + std::to_string(m_distance));
  m_dataCollector.DescribeDouble("Distance", DoubleValue(m_distance));

  if (m_formatType == "omnet") {
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("wifi_performance.omnet");
    m_dataCollector.WriteOmnet(stream);
  } else if (m_formatType == "sqlite") {
    m_dataCollector.WriteSqliteFile("wifi_performance.sqlite");
  }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
}

int main(int argc, char *argv[]) {
  double distance = 50.0;
  std::string formatType = "omnet";
  uint32_t runId = 1;

  CommandLine cmd(__FILE__);
  cmd.AddValue("distance", "Distance between nodes in meters", distance);
  cmd.AddValue("format", "Output format: 'omnet' or 'sqlite'", formatType);
  cmd.AddValue("runId", "Run identifier for experiment", runId);
  cmd.Parse(argc, argv);

  ExperimentHelper helper(distance, formatType, runId);
  helper.Run();

  return 0;
}