#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot.h"
#include "ns3/packet-socket-address.h"
#include <vector>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocWifiExperiment");

class Experiment
{
public:
  Experiment(std::string phyMode, std::string wifiManager, std::string gnuplotOutputFile);
  void Configure();
  void Run();
  void Reset();
  Vector GetPosition(Ptr<Node> node);
  void SetPosition(Ptr<Node> node, Vector position);
  void MoveNode(uint32_t nodeId, Vector position, Time at);
  void ReceivePacket(Ptr<Socket> socket);
  void SetupPacketReceive(Ptr<Node> node);
  void AddThroughputDatum(double distance, double throughput);

  std::string m_phyMode;
  std::string m_wifiManager;
  std::string m_gnuplotFile;

  NodeContainer nodes;
  NetDeviceContainer devices;
  MobilityHelper mobility;
  Gnuplot m_gnuplot;
  Gnuplot2dDataset m_dataset;
  uint64_t m_bytesTotal;
  Time m_lastRxTime;
  uint32_t m_lastRxBytes;
};

Experiment::Experiment(std::string phyMode, std::string wifiManager, std::string gnuplotFile)
  : m_phyMode(phyMode), m_wifiManager(wifiManager), m_gnuplotFile(gnuplotFile), m_bytesTotal(0), m_lastRxTime(Seconds(0)), m_lastRxBytes(0),
    m_gnuplot(gnuplotFile), m_dataset("Throughput vs. Distance")
{
  m_dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);
}

void
Experiment::Configure()
{
  nodes.Create(2);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());
  wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager(m_wifiManager, "DataMode", StringValue(m_phyMode), "ControlMode", StringValue("DsssRate1Mbps"));

  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
  wifiMac.SetType("ns3::AdhocWifiMac");

  devices = wifi.Install(wifiPhy, wifiMac, nodes);

  PacketSocketHelper packetSocket;
  packetSocket.Install(nodes);

  // Mobility
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  Ptr<ConstantPositionMobilityModel> mob0 = nodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
  Ptr<ConstantPositionMobilityModel> mob1 = nodes.Get(1)->GetObject<ConstantPositionMobilityModel>();
  mob0->SetPosition(Vector(0.0, 0.0, 0.0));
  mob1->SetPosition(Vector(0.0, 0.0, 0.0));

  m_bytesTotal = 0;
  m_lastRxTime = Seconds(0.0);
  m_lastRxBytes = 0;
}

Vector
Experiment::GetPosition(Ptr<Node> node)
{
  Ptr<MobilityModel> mob = node->GetObject<MobilityModel>();
  return mob->GetPosition();
}

void
Experiment::SetPosition(Ptr<Node> node, Vector position)
{
  Ptr<MobilityModel> mob = node->GetObject<MobilityModel>();
  mob->SetPosition(position);
}

void
Experiment::MoveNode(uint32_t nodeId, Vector position, Time at)
{
  Simulator::Schedule(at, &Experiment::SetPosition, this, nodes.Get(nodeId), position);
}

void
Experiment::ReceivePacket(Ptr<Socket> socket)
{
  while (Ptr<Packet> pkt = socket->Recv())
    {
      m_bytesTotal += pkt->GetSize();
    }
}

void
Experiment::SetupPacketReceive(Ptr<Node> node)
{
  TypeId tid = TypeId::LookupByName("ns3::PacketSocketFactory");
  Ptr<Socket> recvSink = Socket::CreateSocket(node, tid);
  PacketSocketAddress addr;
  addr.SetSingleDevice(devices.Get(1)->GetIfIndex());
  addr.SetPhysicalAddress(devices.Get(1)->GetAddress());
  addr.SetProtocol(0x807);
  recvSink->Bind(addr);
  recvSink->SetRecvCallback(MakeCallback(&Experiment::ReceivePacket, this));
}

void
Experiment::AddThroughputDatum(double distance, double throughput)
{
  m_dataset.Add(distance, throughput);
}

void
Experiment::Run()
{
  Configure();

  double distances[] = { 10.0, 30.0, 50.0, 70.0, 90.0, 110.0, 130.0, 150.0, 170.0, 190.0 };
  uint32_t numDistances = sizeof(distances) / sizeof(double);
  double simulationTime = 10.0; // seconds
  double appStart = 1.0;
  double appStop = simulationTime - 1.0;

  // Application: OnOff
  PacketSocketAddress socketAddr;
  socketAddr.SetSingleDevice(devices.Get(1)->GetIfIndex());
  socketAddr.SetPhysicalAddress(devices.Get(1)->GetAddress());
  socketAddr.SetProtocol(0x807);

  OnOffHelper onoff("ns3::PacketSocketFactory", Address(socketAddr));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("PacketSize", UintegerValue(1024));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));

  for (uint32_t i = 0; i < numDistances; ++i)
    {
      // Reset counters
      m_bytesTotal = 0;
      m_lastRxTime = Seconds(0.0);
      m_lastRxBytes = 0;

      // Place node 0 at (0,0), node 1 at (distance,0)
      SetPosition(nodes.Get(0), Vector(0.0, 0.0, 0.0));
      SetPosition(nodes.Get(1), Vector(distances[i], 0.0, 0.0));

      // Remove existing applications
      for (uint32_t j = 0; j < nodes.GetN(); ++j)
        {
          nodes.Get(j)->RemoveApplications();
        }

      // Setup receiver on node 1
      SetupPacketReceive(nodes.Get(1));

      // Setup sender on node 0
      ApplicationContainer app = onoff.Install(nodes.Get(0));
      app.Start(Seconds(appStart));
      app.Stop(Seconds(appStop));

      Simulator::Stop(Seconds(simulationTime));
      Simulator::Run();

      // Throughput calculation (bits/sec)
      double throughput = m_bytesTotal * 8.0 / (appStop - appStart) / 1e6; // Mbps
      AddThroughputDatum(distances[i], throughput);

      // Remove packet receiving socket
      nodes.Get(1)->GetObject<ObjectBase>()->AggregateObject(nullptr);
    }

  // Gnuplot output
  m_gnuplot.AddDataset(m_dataset);
  std::ofstream plotFile(m_gnuplotFile);
  m_gnuplot.GenerateOutput(plotFile);
  plotFile.close();

  Simulator::Destroy();
}

void
Experiment::Reset()
{
  nodes = NodeContainer();
  devices = NetDeviceContainer();
}

int
main(int argc, char *argv[])
{
  LogComponentEnable("AdhocWifiExperiment", LOG_LEVEL_INFO);

  std::vector<std::string> phyModes = { "DsssRate1Mbps", "DsssRate11Mbps" };
  std::vector<std::string> wifiManagers = { "ns3::AarfWifiManager", "ns3::ConstantRateWifiManager" };
  std::vector<std::string> gnuplotFiles = { "output_1mbps_aarf.plt", "output_1mbps_constant.plt", "output_11mbps_aarf.plt", "output_11mbps_constant.plt" };

  CommandLine cmd;
  cmd.Parse(argc, argv);

  uint32_t idx = 0;
  for (uint32_t i = 0; i < phyModes.size(); ++i)
    {
      for (uint32_t j = 0; j < wifiManagers.size(); ++j)
        {
          NS_LOG_INFO("Running experiment: " << phyModes[i] << ", " << wifiManagers[j]);
          Experiment exp(phyModes[i], wifiManagers[j], gnuplotFiles[idx++]);
          exp.Run();
          exp.Reset();
        }
    }

  return 0;
}