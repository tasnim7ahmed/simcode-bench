#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/packet-socket-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocWifiExperiment");

class Experiment
{
public:
  Experiment ();
  ~Experiment ();

  void SetPosition (Ptr<Node> node, double x, double y);
  Vector GetPosition (Ptr<Node> node);
  void MoveNode (Ptr<Node> node, double x, double y, double duration);
  void ReceivePacket (Ptr<Socket> socket);
  void SetupPacketReceptionSocket (Ptr<Node> node);
  void Run (std::string wifiRate, std::string rtsCtsEnabled, std::string manager);

private:
  NodeContainer nodes;
  NetDeviceContainer devices;
  Ipv4InterfaceContainer interfaces;
  uint16_t port;
  std::vector<double> throughputValues;
};

Experiment::Experiment () : port (9)
{
}

Experiment::~Experiment ()
{
}

void
Experiment::SetPosition (Ptr<Node> node, double x, double y)
{
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> ();
  mobility->SetPosition (Vector (x, y, 0));
}

Vector
Experiment::GetPosition (Ptr<Node> node)
{
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> ();
  return mobility->GetPosition ();
}

void
Experiment::MoveNode (Ptr<Node> node, double x, double y, double duration)
{
  Ptr<ConstantPositionMobilityModel> mobility = node->GetObject<ConstantPositionMobilityModel> ();
  mobility->SetPosition (Vector (x, y, 0));
}

void
Experiment::ReceivePacket (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet = socket->RecvFrom (from);
  double throughput = (packet->GetSize () * 8.0) / 1.0;
  throughputValues.push_back(throughput);
}

void
Experiment::SetupPacketReceptionSocket (Ptr<Node> node)
{
  TypeId tid = TypeId::LookupByName ("ns3::PacketSocketFactory");
  Ptr<Socket> socket = Socket::CreateSocket (node, tid);
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), port);
  socket->Bind (local);
  socket->SetRecvCallback (MakeCallback (&Experiment::ReceivePacket, this));
}

void
Experiment::Run (std::string wifiRate, std::string rtsCtsEnabled, std::string manager)
{
  throughputValues.clear();
  nodes.Create (2);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  Ssid ssid = Ssid ("adhoc-wifi");
  wifiMac.SetType ("ns3::AdhocWifiMac",
                   "Ssid", SsidValue (ssid));

  devices = wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  SetPosition (nodes.Get (0), 0.0, 0.0);
  SetPosition (nodes.Get (1), 10.0, 0.0);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  interfaces = ipv4.Assign (devices);

  SetupPacketReceptionSocket (nodes.Get (1));

  OnOffHelper onOffHelper ("ns3::PacketSocketFactory",
                             Address (InetSocketAddress (interfaces.GetAddress (1), port)));
  onOffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onOffHelper.SetAttribute ("PacketSize", UintegerValue (1024));
  onOffHelper.SetAttribute ("DataRate", StringValue (wifiRate));

  ApplicationContainer apps = onOffHelper.Install (nodes.Get (0));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (11.0));

  Simulator::Stop (Seconds (11.0));

  Simulator::Run ();

  std::string filename = "throughput-" + wifiRate + "-" + rtsCtsEnabled + "-" + manager + ".plt";
  Gnuplot2dDataset dataset;
  dataset.SetTitle ("Throughput vs. Time");
  dataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  for(size_t i = 0; i < throughputValues.size(); ++i) {
      dataset.Add((double)(i + 1), throughputValues[i]);
  }

  Gnuplot plot (filename);
  plot.AddDataset (dataset);
  plot.GenerateOutput ();

  Simulator::Destroy ();
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  std::vector<std::string> wifiRates = {"54Mbps", "11Mbps"};
  std::vector<std::string> rtsCtsEnabledValues = {"true", "false"};
  std::vector<std::string> managers = {"AarfWifiManager", "AparfWifiManager"};

  for (const auto& wifiRate : wifiRates)
  {
    for (const auto& rtsCtsEnabled : rtsCtsEnabledValues)
    {
      for (const auto& manager : managers)
      {
        Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode",
                             StringValue (manager));
        Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold",
                             StringValue (rtsCtsEnabled == "true" ? "0" : "2200"));

        Experiment experiment;
        experiment.Run (wifiRate, rtsCtsEnabled, manager);
      }
    }
  }

  return 0;
}