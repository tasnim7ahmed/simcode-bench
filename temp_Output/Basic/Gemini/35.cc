#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/nist-error-rate-model.h"
#include "ns3/yans-error-rate-model.h"
#include "ns3/table-error-rate-model.h"
#include "ns3/command-line.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiErrorRateModels");

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t packetSize = 1024; // bytes
  std::string phyMode ("OfdmRate6Mbps");
  double simulationTime = 10; // seconds
  double distance = 100; // meters
  std::string rateInfoFileName = "ofdm-rates.dat";
  double snrStart = -5;
  double snrEnd = 30;
  double snrStep = 1;

  CommandLine cmd;
  cmd.AddValue ("packetSize", "Size of packet in bytes", packetSize);
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("distance", "Distance between nodes", distance);
  cmd.AddValue ("rateInfoFile", "File to read Rate information from", rateInfoFileName);
  cmd.AddValue ("snrStart", "Start SNR", snrStart);
  cmd.AddValue ("snrEnd", "End SNR", snrEnd);
  cmd.AddValue ("snrStep", "SNR Step", snrStep);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (2);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211a);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.Set ("RxGain", DoubleValue (-10) );
  wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");

  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");
  Ssid ssid = Ssid ("ns-3-ssid");
  wifiMac.SetType ("ns3::AdhocWifiMac",
                   "Ssid", SsidValue (ssid));

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (distance),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (simulationTime + 1));

  UdpEchoClientHelper echoClient (i.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.001)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (simulationTime));

  Ptr<OutputStreamWrapper> outputNist = Create<OutputStreamWrapper> ("nist.dat", std::ios::out);
  Ptr<OutputStreamWrapper> outputYans = Create<OutputStreamWrapper> ("yans.dat", std::ios::out);
  Ptr<OutputStreamWrapper> outputTable = Create<OutputStreamWrapper> ("table.dat", std::ios::out);

  std::vector<double> snrValues;
  for (double snr = snrStart; snr <= snrEnd; snr += snrStep)
    {
      snrValues.push_back (snr);
    }

  for (double snr : snrValues)
    {
      Config::Set ("/ChannelList/*/$ns3::YansWifiChannel/PropagationLossModel/$ns3::FriisPropagationLossModel/Frequency", DoubleValue (5.18e9));
      Config::Set ("/ChannelList/*/$ns3::YansWifiChannel/PropagationLossModel/$ns3::FriisPropagationLossModel/SystemLoss", DoubleValue (1.0));
      Config::Set ("/ChannelList/*/$ns3::YansWifiChannel/PropagationLossModel/$ns3::FriisPropagationLossModel/Gain", DoubleValue (0.0));
      Config::Set ("/ChannelList/*/$ns3::YansWifiChannel/PropagationLossModel/$ns3::FriisPropagationLossModel/Lambda", DoubleValue (0.0579));
      Config::Set ("/ChannelList/*/$ns3::YansWifiChannel/PropagationLossModel/$ns3::FriisPropagationLossModel/AntennaHeight1", DoubleValue (1.5));
      Config::Set ("/ChannelList/*/$ns3::YansWifiChannel/PropagationLossModel/$ns3::FriisPropagationLossModel/AntennaHeight2", DoubleValue (1.5));

      // NIST
      NistErrorRateModel nistErrorRateModel;
      wifiPhy.SetErrorRateModel ("ns3::NistErrorRateModel");
      nistErrorRateModel.Set ("Frequency", DoubleValue (5.18e9));
      nistErrorRateModel.Set ("ChannelNumber", IntegerValue (36));

      //SNR Calculation
      double txPowerDbm = 16.0206;
      double noiseFloorDbM = -93.97;
      double distanceLossdB = 20 * log10 (4 * M_PI * distance * 5.18e9 / 299792458) ;
      double rxPowerdBm = txPowerDbm - distanceLossdB;

      double rssiDbm = rxPowerdBm ;
      double sinr = rssiDbm - noiseFloorDbM + snr;
      nistErrorRateModel.Set ("Sinr", DoubleValue (sinr));
      Simulator::Stop (Seconds (simulationTime + 2));
      Simulator::Run ();

      Ptr<UdpEchoClient> client = DynamicCast<UdpEchoClient> (clientApps.Get (0));
      uint32_t packetsSent = client->GetPacketsSent ();
      uint32_t packetsReceived = DynamicCast<UdpEchoServer> (serverApps.Get (0))->GetReceived ();
      double frameSuccessRateNist = (double) packetsReceived / packetsSent;

      Simulator::Destroy ();

      // YANS
      wifiPhy.SetErrorRateModel ("ns3::YansErrorRateModel");
      YansErrorRateModel yansErrorRateModel;
      yansErrorRateModel.Set ("Sinr", DoubleValue (sinr));

      Simulator::Stop (Seconds (simulationTime + 2));
      Simulator::Run ();

      client = DynamicCast<UdpEchoClient> (clientApps.Get (0));
      packetsSent = client->GetPacketsSent ();
      packetsReceived = DynamicCast<UdpEchoServer> (serverApps.Get (0))->GetReceived ();

      double frameSuccessRateYans = (double) packetsReceived / packetsSent;

      Simulator::Destroy ();

      // TABLE
      wifiPhy.SetErrorRateModel ("ns3::TableErrorRateModel");
      TableErrorRateModel tableErrorRateModel;
      tableErrorRateModel.SetRateInfoFileName (rateInfoFileName);
      tableErrorRateModel.SetDefaultPhysRate (phyMode);
      tableErrorRateModel.Set ("Sinr", DoubleValue (sinr));

      Simulator::Stop (Seconds (simulationTime + 2));
      Simulator::Run ();
      client = DynamicCast<UdpEchoClient> (clientApps.Get (0));
      packetsSent = client->GetPacketsSent ();
      packetsReceived = DynamicCast<UdpEchoServer> (serverApps.Get (0))->GetReceived ();

      double frameSuccessRateTable = (double) packetsReceived / packetsSent;
      Simulator::Destroy ();

      *outputNist->GetStream () << snr << " " << frameSuccessRateNist << std::endl;
      *outputYans->GetStream () << snr << " " << frameSuccessRateYans << std::endl;
      *outputTable->GetStream () << snr << " " << frameSuccessRateTable << std::endl;

      nodes.Get (0)->Dispose ();
      nodes.Get (1)->Dispose ();
      devices.Get (0)->Dispose ();
      devices.Get (1)->Dispose ();

      nodes = NodeContainer ();
      nodes.Create (2);
      devices = wifi.Install (wifiPhy, wifiMac, nodes);
      mobility.Install (nodes);
      internet.Install (nodes);
      i = ipv4.Assign (devices);

      clientApps = echoClient.Install (nodes.Get (0));
      serverApps = echoServer.Install (nodes.Get (1));
      serverApps.Start (Seconds (0.0));
      serverApps.Stop (Seconds (simulationTime + 1));
      clientApps.Start (Seconds (1.0));
      clientApps.Stop (Seconds (simulationTime));

    }

  Gnuplot plot;
  plot.SetTitle ("Frame Success Rate vs. SNR");
  plot.SetLegend ("NIST", "YANS", "TABLE");
  plot.SetTerminal ("png");
  plot.SetOutputFileName ("error-rate-models.png");
  plot.AddDataset ("nist.dat", "lines");
  plot.AddDataset ("yans.dat", "lines");
  plot.AddDataset ("table.dat", "lines");

  plot.GenerateOutput ();

  return 0;
}