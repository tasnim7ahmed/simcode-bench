#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"

#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiClearChannelExperiment");

class Experiment
{
public:
  Experiment ();
  ~Experiment ();

  void Run (std::string rate, double distance);

private:
  void SetupNodes ();
  void SetupDevices (std::string rate);
  void SetupMobility (double distance);
  void SetupApplications ();
  void ReceivePacket (std::string context, Ptr<const Packet> packet, uint16_t protocol, const Address &sender);
  void ConfigurePHY (std::string rate);
  void CollectResults ();
  void GenerateGnuplot ();

  NodeContainer nodes;
  NetDeviceContainer devices;
  Ipv4InterfaceContainer interfaces;
  ApplicationContainer apps;
  Ptr<FlowMonitor> flowMonitor;

  uint32_t packetsReceived;
  std::string dataRate;
  double nodeDistance;
  std::ofstream rawData;
};

Experiment::Experiment () : packetsReceived (0)
{
}

Experiment::~Experiment ()
{
  rawData.close ();
}

void
Experiment::Run (std::string rate, double distance)
{
  dataRate = rate;
  nodeDistance = distance;

  std::stringstream ss;
  ss << "wifi_clear_channel_" << rate << "_" << distance << ".dat";
  rawData.open (ss.str().c_str());
  rawData << "# RSS\tPackets Received\n";

  SetupNodes ();
  SetupDevices (rate);
  SetupMobility (distance);
  SetupInternetStack (nodes);
  SetupApplications ();
  ConfigurePHY (rate);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  CollectResults ();
  GenerateGnuplot ();

  Simulator::Destroy ();
}

void
Experiment::SetupNodes ()
{
  nodes.Create (2);
}

void
Experiment::SetupDevices (std::string rate)
{
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Create ();
  phy.SetChannel (channel.Create ());
  phy.Set ("RxGain", DoubleValue(0));

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::StaWifiMac",
                 "Ssid", SsidValue (ssid),
                 "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, nodes.Get (1));

  mac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, nodes.Get (0));

  devices.Add (staDevices);
  devices.Add (apDevices);
}

void
Experiment::SetupMobility (double distance)
{
  MobilityHelper mobility;

  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (distance, 0.0, 0.0));

  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);
}

void
Experiment::SetupApplications ()
{
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  interfaces = address.Assign (devices);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1000));
  echoClient.SetAttribute ("Interval", TimeValue (MilliSeconds (1)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1472));
  apps = echoClient.Install (nodes.Get (1));
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (10.0));

  Config::Connect ("/NodeList/1/ApplicationList/0/$ns3::UdpEchoClient/Rx", MakeCallback (&Experiment::ReceivePacket, this));
}

void
Experiment::ReceivePacket (std::string context, Ptr<const Packet> packet, uint16_t protocol, const Address &sender)
{
  packetsReceived++;
}

void
Experiment::ConfigurePHY (std::string rate)
{
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxGain", DoubleValue(10));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue(22));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/Frequency", UintegerValue(2412));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ShortGuardEnabled", BooleanValue(true));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerStart", DoubleValue (16));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerEnd", DoubleValue (16));

  std::string phyName = "/NodeList/1/DeviceList/0/$ns3::WifiNetDevice/Phy";
  std::string rxDropAttrName = "MonitorSnr";
  Config::ConnectWithoutContext (phyName + "/ReportRx", MakeBoundCallback (&Experiment::CollectResults, this));

  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelNumber", UintegerValue (1));

  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/NonUnicastMode",
               StringValue (rate));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/FragmentationThreshold",
               UintegerValue (2346));
}


void
Experiment::CollectResults ()
{
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  Ptr<YansWifiPhy> recvPhy = DynamicCast<YansWifiPhy> (NodeList::Get (1)->GetDevice (0)->GetPhy ());

  double rss = recvPhy->GetLastRss ();

  rawData << rss << "\t" << packetsReceived << "\n";
  packetsReceived = 0;
}


void
Experiment::GenerateGnuplot ()
{
  std::stringstream ss;
  ss << "wifi_clear_channel_" << dataRate << "_" << nodeDistance;
  std::string plotFileName = ss.str();

  Gnuplot plot (plotFileName + ".eps");
  plot.SetTitle ("WiFi Clear Channel Experiment");
  plot.SetLegend ("RSS", "Packets Received");

  Gnuplot2dDataset dataset;
  dataset.SetTitle ("Data");
  dataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  std::ifstream dataFile ((plotFileName + ".dat").c_str());
  if (dataFile.is_open ())
    {
      double rss, packets;
      while (dataFile >> rss >> packets)
        {
          dataset.Add (rss, packets);
        }
      dataFile.close ();
    }

  plot.AddDataset (dataset);
  plot.GenerateOutput ();
}


int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  std::string rates[] = {"DsssRate1Mbps", "DsssRate2Mbps", "DsssRate5_5Mbps", "DsssRate11Mbps"};
  double distances[] = {1.0, 5.0, 10.0, 20.0};

  for (const std::string& rate : rates)
    {
      for (double distance : distances)
        {
          Experiment experiment;
          experiment.Run (rate, distance);
        }
    }
  return 0;
}