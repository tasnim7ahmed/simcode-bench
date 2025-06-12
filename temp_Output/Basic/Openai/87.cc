#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/data-collector.h"
#include "ns3/omnet-data-output.h"
#include "ns3/sqlite-output.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiDistancePerformanceExperiment");

struct ExperimentStats
{
  uint32_t txPackets = 0;
  uint32_t rxPackets = 0;
  uint64_t totalDelayUs = 0;
};

ExperimentStats stats;

void TxCallback(Ptr<const Packet> packet)
{
  stats.txPackets++;
}

void RxCallback(Ptr<const Packet> packet, const Address &addr)
{
  stats.rxPackets++;
}

void RxTraceWithContext(std::string context, Ptr<const Packet> packet, const Address &addr)
{
  stats.rxPackets++;
}

std::map<uint64_t, Time> sendTimestamps;

void SendTimeTrace(std::string context, Ptr<const Packet> packet)
{
  sendTimestamps[packet->GetUid()] = Simulator::Now();
  stats.txPackets++;
}

void ReceiveTimeTrace(std::string context, Ptr<const Packet> packet, const Address& address)
{
  auto it = sendTimestamps.find(packet->GetUid());
  if (it != sendTimestamps.end())
  {
    Time tx = it->second;
    Time rx = Simulator::Now();
    stats.totalDelayUs += (rx - tx).GetMicroSeconds();
    stats.rxPackets++;
  }
}

int main(int argc, char *argv[])
{
  double distance = 50.0;
  std::string outputFormat = "omnet";
  uint32_t run = 1;
  uint32_t payloadSize = 1024;
  double simulationTime = 10.0; // seconds
  double dataRateMbps = 6.0; // Mbps

  CommandLine cmd;
  cmd.AddValue("distance", "Separation between Wi-Fi nodes (meters)", distance);
  cmd.AddValue("format", "Data output format: omnet|sqlite", outputFormat);
  cmd.AddValue("run", "Run ID", run);
  cmd.Parse(argc, argv);

  RngSeedManager::SetRun(run);

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211g);

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  mac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0));
  positionAlloc->Add(Vector(distance, 0.0, 0.0));
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Install UDP traffic from node 0 to node 1
  uint16_t port = 5000;
  UdpServerHelper server(port);
  ApplicationContainer serverApp = server.Install(nodes.Get(1));
  serverApp.Start(Seconds(0));
  serverApp.Stop(Seconds(simulationTime + 1));

  UdpClientHelper client(interfaces.GetAddress(1), port);
  client.SetAttribute("MaxPackets", UintegerValue(10000000));
  client.SetAttribute("Interval", TimeValue(Seconds(double(payloadSize*8) / (dataRateMbps*1000000.0))));
  client.SetAttribute("PacketSize", UintegerValue(payloadSize));
  ApplicationContainer clientApp = client.Install(nodes.Get(0));
  clientApp.Start(Seconds(1));
  clientApp.Stop(Seconds(simulationTime));

  // Tracing
  Config::Connect("/NodeList/0/ApplicationList/0/$ns3::UdpClient/Tx", MakeCallback(&SendTimeTrace));
  Config::Connect("/NodeList/1/ApplicationList/0/$ns3::UdpServer/Rx", MakeCallback(&ReceiveTimeTrace));

  // DataCollector setup
  Ptr<DataCollector> collector = CreateObject<DataCollector>();
  collector->SetExperimentId("wifi-distance");
  collector->SetRunId(std::to_string(run));
  collector->SetDescription("Measure Wi-Fi Adhoc throughput, delay, and packet loss for 2 nodes, variable distance");

  collector->AddMetadata("author", "ns-3 experimenter");
  collector->AddMetadata("distance", std::to_string(distance));
  collector->AddMetadata("payloadSize", std::to_string(payloadSize));
  collector->AddMetadata("dataRateMbps", std::to_string(dataRateMbps));
  collector->AddMetadata("format", outputFormat);

  // Simulation
  Simulator::Stop(Seconds(simulationTime + 1));
  Simulator::Run();
  double avgDelayUs = (stats.rxPackets > 0) ? double(stats.totalDelayUs)/stats.rxPackets : 0.0;
  double lossRatio = (stats.txPackets > 0) ? double(stats.txPackets - stats.rxPackets)/stats.txPackets : 0.0;

  collector->AddData("distance", distance);
  collector->AddData("payloadSize", payloadSize);
  collector->AddData("dataRateMbps", dataRateMbps);
  collector->AddData("txPackets", stats.txPackets);
  collector->AddData("rxPackets", stats.rxPackets);
  collector->AddData("lossRatio", lossRatio);
  collector->AddData("avgDelayUs", avgDelayUs);

  // Output results
  if (outputFormat == "sqlite")
  {
    Ptr<SQLiteDataOutput> output = CreateObject<SQLiteDataOutput>();
    output->SetFilePrefix("wifi-distance-results");
    output->Output(*collector);
  }
  else // default to OMNeT
  {
    Ptr<OmnetDataOutput> output = CreateObject<OmnetDataOutput>();
    output->SetFilePrefix("wifi-distance-results");
    output->Output(*collector);
  }

  Simulator::Destroy();
  return 0;
}