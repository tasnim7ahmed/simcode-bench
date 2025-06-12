#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/command-line.h"
#include "ns3/gnuplot.h"
#include "ns3/wifi-phy.h"
#include "ns3/nist-error-rate-model.h"
#include "ns3/yans-error-rate-model.h"
#include "ns3/table-error-rate-model.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/constant-position-mobility-model.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VhtErrorRateValidation");

int main (int argc, char *argv[])
{
  bool enableNist = true;
  bool enableYans = true;
  bool enableTable = true;
  uint32_t frameSize = 1000;

  CommandLine cmd;
  cmd.AddValue ("enableNist", "Enable NIST error rate model", enableNist);
  cmd.AddValue ("enableYans", "Enable YANS error rate model", enableYans);
  cmd.AddValue ("enableTable", "Enable Table error rate model", enableTable);
  cmd.AddValue ("frameSize", "Frame size in bytes", frameSize);
  cmd.Parse (argc, argv);

  LogComponent::SetLevel (LogComponent::GetComponent ("VhtErrorRateValidation"), LOG_LEVEL_INFO);

  // Simulation setup
  NodeContainer nodes;
  nodes.Create (2);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211ac);

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Simple ();
  channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  channel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("vht-validation");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, nodes.Get (0));

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "BeaconInterval", TimeValue (Seconds (1.0)),
               "EnableBeaconJitter", BooleanValue (false));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, nodes.Get (1));

  // Mobility model
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

  Ptr<ConstantPositionMobilityModel> staMobility = staDevices.Get (0)->GetNode ()->GetObject<ConstantPositionMobilityModel> ();
  staMobility->SetPosition (Vector (0.0, 0.0, 0.0));

  Ptr<ConstantPositionMobilityModel> apMobility = apDevices.Get (0)->GetNode ()->GetObject<ConstantPositionMobilityModel> ();
  apMobility->SetPosition (Vector (1.0, 0.0, 0.0));

  // Iterate through all VHT MCS values (excluding MCS 9 for 20 MHz)
  for (uint32_t mcs = 0; mcs <= 9; ++mcs)
  {
    if (mcs == 9)
    {
      continue; // Skip MCS 9 for 20 MHz
    }

    // Configure VHT rate
    std::stringstream ss;
    ss << "VhtMcs" << mcs;
    std::string rate = ss.str ();
    Config::Set ("/NodeList/*/$ns3::WifiNetDevice/Phy/TxChannelWidth", UintegerValue (20)); // Force 20 MHz for MCS 0-8
    Config::Set ("/NodeList/*/$ns3::WifiNetDevice/Phy/TxAntennas", UintegerValue (1));
    Config::Set ("/NodeList/*/$ns3::WifiNetDevice/Phy/TxSpatialStreams", UintegerValue (1));
    Config::Set ("/NodeList/*/$ns3::WifiNetDevice/Phy/TxVhtMcs", UintegerValue (mcs));
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue (rate),
                                  "ControlMode", StringValue (rate));

    // SNR values
    std::vector<double> snrValues;
    for (double snr = -5.0; snr <= 30.0; snr += 1.0)
    {
      snrValues.push_back (snr);
    }

    // Error rate models
    std::map<std::string, std::vector<double>> frameSuccessRates;
    if (enableNist)
    {
      frameSuccessRates["NIST"] = std::vector<double> (snrValues.size (), 0.0);
    }
    if (enableYans)
    {
      frameSuccessRates["YANS"] = std::vector<double> (snrValues.size (), 0.0);
    }
    if (enableTable)
    {
      frameSuccessRates["Table"] = std::vector<double> (snrValues.size (), 0.0);
    }

    // Loop through SNR values
    for (size_t i = 0; i < snrValues.size (); ++i)
    {
      double snr = snrValues[i];

      // Calculate received power based on SNR
      double noiseFloor = -93.97; // dBm, typical for 20 MHz BW
      double receivedPower = snr + noiseFloor;

      // Update PHY attributes
      Config::Set ("/NodeList/*/$ns3::WifiNetDevice/Phy/RxSensitivity", DoubleValue (noiseFloor));
      phy.Set ("RxSensitivity", DoubleValue (noiseFloor));
      phy.Set ("CcaEdThreshold", DoubleValue (noiseFloor));

      // NIST model
      if (enableNist)
      {
        NistErrorRateModel nistErrorRateModel;
        frameSuccessRates["NIST"][i] = nistErrorRateModel.GetFrameSuccessRate (receivedPower, snr, rate, frameSize, 0);
      }

      // YANS model
      if (enableYans)
      {
        YansErrorRateModel yansErrorRateModel;
        frameSuccessRates["YANS"][i] = yansErrorRateModel.GetFrameSuccessRate (receivedPower, snr, rate, frameSize, 0);
      }

      // Table model
      if (enableTable)
      {
        TableErrorRateModel tableErrorRateModel;
        tableErrorRateModel.SetAttribute ("DataRate", StringValue (rate));
        frameSuccessRates["Table"][i] = tableErrorRateModel.GetFrameSuccessRate (receivedPower, snr, rate, frameSize, 0);
      }
    }

    // Gnuplot output
    std::stringstream filenameSs;
    filenameSs << "vht-mcs-" << mcs << "-error-rate.plt";
    std::string filename = filenameSs.str ();

    Gnuplot gnuplot (filename);
    gnuplot.SetTitle ("Frame Success Rate vs. SNR (VHT MCS " + std::to_string (mcs) + ")");
    gnuplot.SetLegend ("SNR (dB)", "Frame Success Rate");

    Gnuplot2dDataset datasetNist ("NIST");
    Gnuplot2dDataset datasetYans ("YANS");
    Gnuplot2dDataset datasetTable ("Table");

    for (size_t i = 0; i < snrValues.size (); ++i)
    {
      double snr = snrValues[i];

      if (enableNist)
      {
        datasetNist.Add (snr, frameSuccessRates["NIST"][i]);
      }
      if (enableYans)
      {
        datasetYans.Add (snr, frameSuccessRates["YANS"][i]);
      }
      if (enableTable)
      {
        datasetTable.Add (snr, frameSuccessRates["Table"][i]);
      }
    }

    if (enableNist)
    {
      gnuplot.AddDataset (datasetNist);
    }
    if (enableYans)
    {
      gnuplot.AddDataset (datasetYans);
    }
    if (enableTable)
    {
      gnuplot.AddDataset (datasetTable);
    }

    std::ofstream plotFile (filename.c_str ());
    gnuplot.GenerateOutput (plotFile);
    plotFile.close ();

    std::cout << "Generated plot: " << filename << std::endl;
  }

  Simulator::Destroy ();
  return 0;
}