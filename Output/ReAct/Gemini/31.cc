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

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if enabled.", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing.", tracing);

  cmd.Parse (argc,argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  Config::SetDefault ("ns3::WifiMacQueue::MaxSize", StringValue ("1000"));
  Config::SetDefault ("ns3::WifiMacQueue::Mode", EnumValue (WifiMacQueue::ADAPTIVE));

  NodeContainer staNodes;
  staNodes.Create (1);

  NodeContainer apNodes;
  apNodes.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211_EHT);

  NistErrorRateModelHelper nistErrorModel;
  YansErrorRateModelHelper yansErrorModel;
  TableBasedErrorRateModelHelper tableErrorModel;

  wifi.SetRemoteStationManager ("ns3::HeHtWifiRemoteStationManager",
                               "NonErpPhy", BooleanValue (false),
                               "EnableMbf", BooleanValue (false));

  Ssid ssid = Ssid ("ns3-wifi");

  WifiMacHelper mac;

  mac.SetType ("ns3::StationWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices = wifi.Install (phy, mac, staNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "BeaconGeneration", BooleanValue (true),
               "Erp", BooleanValue (false));

  NetDeviceContainer apDevices = wifi.Install (phy, mac, apNodes);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                               "MinX", DoubleValue (0.0),
                               "MinY", DoubleValue (0.0),
                               "DeltaX", DoubleValue (5.0),
                               "DeltaY", DoubleValue (10.0),
                               "GridWidth", UintegerValue (3),
                               "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  mobility.Install (staNodes);
  mobility.Install (apNodes);

  InternetStackHelper stack;
  stack.Install (apNodes);
  stack.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer staNodeIface;
  Ipv4InterfaceContainer apNodeIface;
  apNodeIface = address.Assign (apDevices);
  address.NewNetwork ();
  staNodeIface = address.Assign (staDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  UdpServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (apNodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpClientHelper echoClient (apNodeIface.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  echoClient.SetAttribute ("Interval", TimeValue (MilliSeconds (1)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1472));

  ApplicationContainer clientApps = echoClient.Install (staNodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  phy.SetErrorRateModel ("ns3::NistErrorRateModel");
  std::string nistOutputFileName = "nist_fsr_vs_snr.plt";
  std::string nistOutputTitle = "NIST Error Rate Model";

  phy.SetErrorRateModel ("ns3::YansErrorRateModel");
  std::string yansOutputFileName = "yans_fsr_vs_snr.plt";
  std::string yansOutputTitle = "YANS Error Rate Model";

  phy.SetErrorRateModel ("ns3::TableBasedErrorRateModel");
  std::string tableOutputFileName = "table_fsr_vs_snr.plt";
  std::string tableOutputTitle = "Table Based Error Rate Model";

  std::vector<std::string> modelNames = {"Nist", "Yans", "Table"};
  std::vector<std::string> outputFiles = {nistOutputFileName, yansOutputFileName, tableOutputFileName};
  std::vector<std::string> outputTitles = {nistOutputTitle, yansOutputTitle, tableOutputTitle};

  for (size_t modelIdx = 0; modelIdx < modelNames.size(); ++modelIdx)
  {
      if(modelNames[modelIdx] == "Nist")
          phy.SetErrorRateModel ("ns3::NistErrorRateModel");
      else if(modelNames[modelIdx] == "Yans")
          phy.SetErrorRateModel ("ns3::YansErrorRateModel");
      else
          phy.SetErrorRateModel ("ns3::TableBasedErrorRateModel");

      Gnuplot2dDataset dataset;
      Gnuplot plot;

      plot.SetTerminal ("png");
      plot.SetLegend ("SNR (dB)", "Frame Success Rate");
      plot.AppendExtra ("set xrange [5:40]");
      plot.AppendExtra ("set yrange [0:1]");
      plot.SetTitle (outputTitles[modelIdx]);
      plot.SetOutputFileName (outputFiles[modelIdx]);

      for (int mcs = 0; mcs <= 11; ++mcs)
      {
          std::vector<std::pair<double, double>> fsr_vs_snr;
          for (double snr = 5; snr <= 40; snr += 1)
          {
            phy.Set ("Snr", DoubleValue(snr));
            Simulator::Stop (Seconds (10.0));
            Simulator::Run ();

            uint32_t totalTx = phy.GetPhyStats ().GetTotalTxPackets ();
            uint32_t totalRx = phy.GetPhyStats ().GetTotalRxPackets ();
            double fsr = (totalTx > 0) ? (double)totalRx / (double)totalTx : 0.0;

            fsr_vs_snr.push_back (std::make_pair (snr, fsr));
            phy.ResetStats ();
            Simulator::Destroy ();
            serverApps.Start (Seconds (1.0));
            serverApps.Stop (Seconds (10.0));
            clientApps.Start (Seconds (2.0));
            clientApps.Stop (Seconds (10.0));
            Simulator::Schedule(Seconds(0.5), [](){Ipv4GlobalRoutingHelper::RecomputeRoutingTables();}); //Ensure routing is up to date

          }

          dataset.SetTitleFormat ("MCS = %d", mcs);
          dataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);
          for (const auto &pair : fsr_vs_snr)
          {
            dataset.Add (pair.first, pair.second);
          }

          plot.AddDataset (dataset);
          dataset.Clear ();
      }
      plot.GenerateOutput ();
   }

  if (tracing)
    {
      phy.EnablePcap ("nist", apDevices.Get (0));
    }

  return 0;
}