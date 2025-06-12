#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/command-line.h"
#include "ns3/spectrum-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/propagation-module.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HeErrorRateModels");

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t frameSize = 12000; // bits
  double simulationTime = 10;  // seconds
  std::string phyMode ("HeMcs7");

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("frameSize", "Frame size in bits", frameSize);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("HeErrorRateModels", LOG_LEVEL_INFO);
    }

  // Disable fragmentation since frame error rate is computed on full frames
  Config::SetDefault ("ns3::WifiMacQueue::FragmentationThreshold", StringValue ("999999"));
  Config::SetDefault ("ns3::WifiMacQueue::RtsCtsThreshold", StringValue ("999999"));

  NodeContainer staNodes;
  staNodes.Create (1);
  NodeContainer apNodes;
  apNodes.Create (1);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211_HE);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-he");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, staNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "BeaconInterval", TimeValue (MicroSeconds (102400)),
               "DtimPeriod", UintegerValue (2));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, apNodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apNodes);
  mobility.Install (staNodes);

  InternetStackHelper stack;
  stack.Install (apNodes);
  stack.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staNodeInterface;
  Ipv4InterfaceContainer apNodeInterface;
  staNodeInterface = address.Assign (staDevices);
  apNodeInterface = address.Assign (apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;

  UdpClientHelper client (apNodeInterface.GetAddress (0), port);
  client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client.SetAttribute ("Interval", TimeValue (Time (Seconds (0.00001)))); //packets/sec (or bits/sec)
  client.SetAttribute ("PacketSize", UintegerValue (frameSize / 8));
  ApplicationContainer clientApps = client.Install (staNodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (simulationTime + 1));

  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (apNodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime + 1));


  std::vector<std::string> errorRateModelNames = {"NistErrorRateModel", "YansErrorRateModel", "TableBasedErrorRateModel"};
  std::vector<std::string> mcsValues = {"HeMcs0", "HeMcs1", "HeMcs2", "HeMcs3", "HeMcs4", "HeMcs5", "HeMcs6", "HeMcs7", "HeMcs8", "HeMcs9", "HeMcs10", "HeMcs11"};
  std::vector<double> snrValues;
  for (double snr = 5.0; snr <= 30.0; snr += 1.0)
    {
      snrValues.push_back (snr);
    }

  for (const auto& modelName : errorRateModelNames)
    {
      for (const auto& mcs : mcsValues)
        {
          std::ofstream outputStream (modelName + "_" + mcs + ".dat");
          outputStream << "# SNR FrameSuccessRate" << std::endl;

          for (double snr : snrValues)
            {
              Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (20));
              Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ShortGuardEnabled", BooleanValue (false));
              Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/GuardInterval", TimeValue (NanoSeconds (800)));
              Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/HeMuDisable", BooleanValue (true));
              Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/HeSuDisable", BooleanValue (false));

              Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ErrorRateModel", StringValue (modelName));
              Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/HeErrorRateModel/BerTableNum", UintegerValue (0));
              Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerStart", DoubleValue (16.0206));
              Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerEnd", DoubleValue (16.0206));

              Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/MinChannelWidth", UintegerValue (20));
              Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/SupportedChannelWidthSet", StringValue ("20"));

              Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/HeErrorRateModel/Mcs", StringValue (mcs));

              // Set the SNR for the simulation
              Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/Snr", DoubleValue (snr));

              Simulator::Stop (Seconds (simulationTime));
              Simulator::Run ();

              Ptr<UdpServer> serverApp = DynamicCast<UdpServer> (serverApps.Get (0));
              uint64_t totalPacketsReceived = serverApp->GetReceived ();

              double frameSuccessRate = (double)totalPacketsReceived / ((double) simulationTime / 0.00001);

              outputStream << snr << " " << frameSuccessRate << std::endl;

              Simulator::Destroy ();
            }

          outputStream.close ();
        }
    }

  return 0;
}