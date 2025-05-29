#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/packet-socket-address.h"
#include <vector>
#include <string>
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiAdhocExperiment");

class Experiment
{
public:
  Experiment(const std::string &dataRate,
             const std::string &wifiManager,
             Gnuplot2dDataset &dataset);
  void Run();
private:
  void CreateNodes();
  void SetupWifi();
  void SetupMobility();
  void SetupSockets();
  void InstallApplications();
  void MoveNodes();
  void LogThroughput();
  Vector GetNodePosition(Ptr<Node> node);
  void SetNodePosition(Ptr<Node> node, Vector pos);
  void ReceivePacket(Ptr<Socket> socket);

  std::string m_dataRate;
  std::string m_wifiManager;
  Gnuplot2dDataset &m_dataset;
  NodeContainer m_nodes;
  NetDeviceContainer m_devices;
  std::vector<Ptr<Socket>> m_packetSockets;
  uint32_t m_rxBytes;
  Time m_lastTime;
  uint32_t m_lastRxBytes;
};

Experiment::Experiment(const std::string &dataRate, const std::string &wifiManager, Gnuplot2dDataset &dataset)
  : m_dataRate(dataRate), m_wifiManager(wifiManager), m_dataset(dataset), m_rxBytes(0), m_lastRxBytes(0), m_lastTime(Seconds(0.0))
{
}

void
Experiment::CreateNodes()
{
  m_nodes.Create(2);
}

void
Experiment::SetupWifi()
{
  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
  WifiHelper wifi;
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
  wifi.SetRemoteStationManager(m_wifiManager, "DataMode", StringValue(m_dataRate), "ControlMode", StringValue(m_dataRate));

  mac.SetType("ns3::AdhocWifiMac");
  m_devices = wifi.Install(phy, mac, m_nodes);
}

void
Experiment::SetupMobility()
{
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0));
  positionAlloc->Add(Vector(5.0, 0.0, 0.0));
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(m_nodes);
}

Vector
Experiment::GetNodePosition(Ptr<Node> node)
{
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
  return mobility->GetPosition();
}

void
Experiment::SetNodePosition(Ptr<Node> node, Vector pos)
{
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
  mobility->SetPosition(pos);
}

void
Experiment::MoveNodes()
{
  Vector pos0 = GetNodePosition(m_nodes.Get(0));
  SetNodePosition(m_nodes.Get(0), Vector(pos0.x + 1.0, pos0.y + 0.0, pos0.z));
  Simulator::Schedule(Seconds(1.0), &Experiment::MoveNodes, this);
}

void
Experiment::ReceivePacket(Ptr<Socket> socket)
{
  while (Ptr<Packet> packet = socket->Recv())
  {
    m_rxBytes += packet->GetSize();
  }
}

void
Experiment::LogThroughput()
{
  Time now = Simulator::Now();
  double throughput = (m_rxBytes - m_lastRxBytes) * 8.0 / 1e6; // Mbps over last second
  m_dataset.Add(now.GetSeconds(), throughput);
  m_lastRxBytes = m_rxBytes;
  m_lastTime = now;
  Simulator::Schedule(Seconds(1.0), &Experiment::LogThroughput, this);
}

void
Experiment::SetupSockets()
{
  PacketSocketHelper packetSocket;
  packetSocket.Install(m_nodes);

  for (uint32_t i = 0; i < m_nodes.GetN(); ++i)
  {
    Ptr<PacketSocket> socket = CreateObject<PacketSocket>();
    m_nodes.Get(i)->AggregateObject(socket);
    Ptr<Socket> pktSocket = Socket::CreateSocket(m_nodes.Get(i), PacketSocketFactory::GetTypeId());
    if (i == 1)
    {
      pktSocket->SetRecvCallback(MakeCallback(&Experiment::ReceivePacket, this));
    }
    m_packetSockets.push_back(pktSocket);
  }
}

void
Experiment::InstallApplications()
{
  PacketSocketAddress socketAddr;
  socketAddr.SetSingleDevice(m_devices.Get(1)->GetIfIndex());
  socketAddr.SetPhysicalAddress(m_devices.Get(1)->GetAddress());
  socketAddr.SetProtocol(0);

  OnOffHelper onoff("ns3::PacketSocketFactory", Address(socketAddr));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("2Mbps")));
  onoff.SetAttribute("PacketSize", UintegerValue(1024));
  onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
  onoff.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
  ApplicationContainer apps = onoff.Install(m_nodes.Get(0));
  apps.Start(Seconds(1.));
  apps.Stop(Seconds(10.));
}

void
Experiment::Run()
{
  CreateNodes();
  SetupWifi();
  SetupMobility();
  SetupSockets();
  InstallApplications();

  Simulator::Schedule(Seconds(2.0), &Experiment::MoveNodes, this);
  Simulator::Schedule(Seconds(1.0), &Experiment::LogThroughput, this);

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();
}

int main(int argc, char *argv[])
{
  CommandLine cmd;
  std::string outputPrefix = "adhoc-wifi";
  cmd.AddValue("outputPrefix", "Output prefix for gnuplot data", outputPrefix);
  cmd.Parse(argc, argv);

  std::vector<std::string> managers = {"ns3::AarfWifiManager", "ns3::MinstrelWifiManager"};
  std::vector<std::string> rates = {"DsssRate11Mbps", "DsssRate5_5Mbps"};

  for (const auto &manager : managers)
  {
    for (const auto &rate : rates)
    {
      std::ostringstream datName, pltName, title;
      datName << outputPrefix << "_" << manager.substr(manager.find_last_of(":") + 1)
              << "_" << rate << ".dat";
      pltName << outputPrefix << "_" << manager.substr(manager.find_last_of(":") + 1)
              << "_" << rate << ".plt";

      Gnuplot gnuplot(pltName.str());
      gnuplot.SetTitle("Throughput over Time");
      gnuplot.SetLegend("Time (s)", "Throughput (Mbps)");

      Gnuplot2dDataset dataset;
      dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

      Experiment experiment(rate, manager, dataset);
      experiment.Run();

      gnuplot.AddDataset(dataset);

      std::ofstream datFile(datName.str());
      dataset.SerializeToOutputStream(datFile);
      datFile.close();

      std::ofstream pltFile(pltName.str());
      gnuplot.GenerateOutput(pltFile);
      pltFile.close();
    }
  }
  return 0;
}