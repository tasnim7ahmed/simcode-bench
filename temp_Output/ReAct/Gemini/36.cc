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
#include <vector>
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VhtErrorRateValidation");

int main (int argc, char *argv[])
{
  bool enable_nist = true;
  bool enable_yans = true;
  bool enable_table = true;
  uint32_t frameSize = 12000;

  CommandLine cmd;
  cmd.AddValue ("enableNist", "Enable NIST Error Rate Model", enable_nist);
  cmd.AddValue ("enableYans", "Enable YANS Error Rate Model", enable_yans);
  cmd.AddValue ("enableTable", "Enable Table Error Rate Model", enable_table);
  cmd.AddValue ("frameSize", "Frame size in bytes", frameSize);
  cmd.Parse (argc, argv);

  LogComponent::SetLevel (LogComponent::LookupByName ("VhtErrorRateValidation"), LOG_LEVEL_INFO);

  // Wifi setup
  WifiHelper wifiHelper;
  wifiHelper.SetStandard (WIFI_STANDARD_80211ac);

  YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default ();
  phyHelper.SetChannel (channelHelper.Create ());
  phyHelper.Set ("ChannelWidth", UintegerValue (20));

  WifiMacHelper macHelper;
  Ssid ssid = Ssid ("ns3-vht");
  macHelper.SetType ("ns3::StaWifiMac",
                     "Ssid", SsidValue (ssid),
                     "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  NetDeviceContainer apDevices;

  NodeContainer apNode;
  apNode.Create (1);
  NodeContainer staNode;
  staNode.Create (1);

  macHelper.SetType ("ns3::ApWifiMac",
                     "Ssid", SsidValue (ssid));

  apDevices = wifiHelper.Install (phyHelper, macHelper, apNode);
  staDevices = wifiHelper.Install (phyHelper, macHelper, staNode);

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apNode);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (staNode);

  // Internet stack
  InternetStackHelper internet;
  internet.Install (apNode);
  internet.Install (staNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign (apDevices);
  Ipv4InterfaceContainer staInterface = address.Assign (staDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Install receiver application on the access point
  uint16_t port = 9;
  UdpServerHelper server (port);
  ApplicationContainer serverApp = server.Install (apNode);
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (10.0));

  // Define VHT MCS values (excluding MCS 9 for 20 MHz)
  std::vector<uint8_t> mcsValues = {0, 1, 2, 3, 4, 5, 6, 7, 8};

  // SNR range
  double snrStart = -5.0;
  double snrStop = 30.0;
  double snrStep = 1.0;

  // Loop through error rate models
  std::vector<std::string> errorRateModels;
  if(enable_nist) errorRateModels.push_back("ns3::NistErrorRateModel");
  if(enable_yans) errorRateModels.push_back("ns3::YansErrorRateModel");
  if(enable_table) errorRateModels.push_back("ns3::TableErrorRateModel");

  for (const auto& errorRateModelName : errorRateModels)
    {
      for (uint8_t mcs : mcsValues)
        {
          // Prepare data for Gnuplot
          Gnuplot2dDataset dataset;
          dataset.SetTitle ("MCS " + std::to_string(mcs) + " " + errorRateModelName);
          dataset.SetStyle (Gnuplot2dDataset::LINES);

          // Loop through SNR values
          for (double snr = snrStart; snr <= snrStop; snr += snrStep)
            {
              // Configure sender
              UdpClientHelper client (apInterface.GetAddress (0), port);
              client.SetAttribute ("MaxPackets", UintegerValue (1000));
              client.SetAttribute ("Interval", TimeValue (Seconds (0.00001))); // Adjust for desired packet rate
              client.SetAttribute ("PacketSize", UintegerValue (frameSize));

              ApplicationContainer clientApp = client.Install (staNode);
              clientApp.Start (Seconds (1.0));
              clientApp.Stop (Seconds (9.0));

              // Configure PHY to use the specific MCS and SNR
              Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (20));
              Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/Frequency", UintegerValue (5180));
              Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerStart", DoubleValue (16));
              Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerEnd", DoubleValue (16));
              Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ShortGuardEnabled", BooleanValue (true));
              Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/GuardInterval", TimeValue (NanoSeconds (400)));
              Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ErrorRateModel", StringValue (errorRateModelName));
              Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/Snr", DoubleValue (snr));
              Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Mcs", UintegerValue (mcs));

              // Run simulation for a short time
              Simulator::Stop (Seconds (10.0));
              Simulator::Run ();

              // Calculate Frame Success Rate (FSR)
              Ptr<Application> app = clientApp.Get (0);
              Ptr<UdpClient> udpClient = DynamicCast<UdpClient>(app);
              uint32_t packetsSent = udpClient->GetPacketsSent ();

              app = serverApp.Get (0);
              Ptr<UdpServer> udpServer = DynamicCast<UdpServer>(app);
              uint32_t packetsReceived = udpServer->GetReceived ();

              double fsr = (packetsSent > 0) ? (double)packetsReceived / packetsSent : 0.0;

              dataset.Add (snr, fsr);
              Simulator::Destroy ();
            }

          // Plot FSR vs. SNR using Gnuplot
          std::string filename = "vht-fsr-mcs-" + std::to_string(mcs) + "-" + errorRateModelName + ".plt";
          Gnuplot plot (filename);
          plot.SetTitle ("Frame Success Rate vs. SNR (MCS " + std::to_string(mcs) + ", " + errorRateModelName + ")");
          plot.SetLegend ("SNR (dB)", "Frame Success Rate");
          plot.AddDataset (dataset);
          plot.GenerateOutput ();
        }
    }
  return 0;
}