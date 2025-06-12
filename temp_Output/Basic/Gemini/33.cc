#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ht-wifi-mac.h"
#include "ns3/propagation-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HtErrorRateModels");

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t packetSize = 1472;
  double simulationTime = 10;
  std::string rateInfoFile;

  CommandLine cmd;
  cmd.AddValue ("packetSize", "Size of packet generated", packetSize);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("verbose", "Tell application to log if set to true", verbose);
  cmd.AddValue ("rateInfoFile", "Optional file containing SNR/FER data", rateInfoFile);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("HtErrorRateModels", LOG_LEVEL_INFO);
    }

  std::vector<std::string> errorModels = {"NistErrorRateModel", "YansErrorRateModel", "TableBasedErrorRateModel"};
  std::vector<uint8_t> mcsValues = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  std::vector<double> snrValues = {5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31, 33, 35};

  for (const auto& modelName : errorModels)
    {
      for (uint8_t mcs : mcsValues)
        {
          std::ofstream output(modelName + "-MCS" + std::to_string(mcs) + ".dat");
          output << "# SNR FrameSuccessRate" << std::endl;

          for (double snr : snrValues)
            {
              // Create nodes
              NodeContainer nodes;
              nodes.Create (2);

              // Configure Wi-Fi
              WifiHelper wifi;
              wifi.SetStandard (WIFI_PHY_STANDARD_80211n);

              YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
              wifiPhy.Set ("RxGain", DoubleValue (10));
              wifiPhy.Set ("TxGain", DoubleValue (10));
              wifiPhy.Set ("CcaEdThreshold", DoubleValue (-79));
              wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue (-79));
              wifiPhy.Set ("ChannelNumber", UintegerValue (1));
              wifiPhy.SetErrorRateModel (modelName);

              if (modelName == "TableBasedErrorRateModel") {
                  if (rateInfoFile.empty()) {
                      std::cerr << "TableBasedErrorRateModel requires a rateInfoFile to be specified." << std::endl;
                      return 1;
                  }
                  Config::SetGlobal ("ns3::RateErrorModel::FileName", StringValue (rateInfoFile));
              }
              YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
              wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel");
              wifiChannel.AddPropagationLoss ("ns3::FixedRssLossModel", "Rss", DoubleValue (-100 + snr));
              wifiChannel.AddPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
              wifiPhy.SetChannel (wifiChannel.Create ());

              WifiMacHelper wifiMac;
              wifiMac.SetType ("ns3::AdhocWifiMac");
              NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

              // Configure HT
              Ssid ssid = Ssid ("ht-adhoc");
              wifiMac.SetType ("ns3::AdhocWifiMac",
                                "Ssid", SsidValue (ssid));

              HtWifiMac::HtCapabilitiesInformation capabilities;
              capabilities.enableGreenfield = true;
              capabilities.enableRifs = false;
              capabilities.enableLdpcCoding = true;
              capabilities.enableStbc = true;

              Config::Set ("/NodeList/*/$ns3::WifiNetDevice/Mac/HtConfiguration/HtCapabilities",
                            HtCapabilitiesInformationValue (capabilities));

              Config::Set ("/NodeList/*/$ns3::WifiNetDevice/Mac/HtConfiguration/SupportedTxSpatialStreams",
                            UintegerValue (1));

              Config::Set ("/NodeList/*/$ns3::WifiNetDevice/Mac/HtConfiguration/SupportedRxSpatialStreams",
                            UintegerValue (1));

              Config::Set ("/NodeList/*/$ns3::WifiNetDevice/Mac/HtConfiguration/GreenfieldSupported", BooleanValue (true));

              // Set HT data rate for all devices
              std::stringstream rateStringStream;
              rateStringStream << "HtMcs" << (int) mcs;

              DataRate rate (rateStringStream.str ());
              Config::Set ("/NodeList/*/$ns3::WifiNetDevice/Mac/HtConfiguration/DataRate", DataRateValue (rate));
              Config::Set ("/NodeList/*/$ns3::WifiNetDevice/Mac/HtConfiguration/ControlRate", DataRateValue ("HtMcs0"));
              // Mobility
              MobilityHelper mobility;
              mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                             "MinX", DoubleValue (0.0),
                                             "MinY", DoubleValue (0.0),
                                             "DeltaX", DoubleValue (5.0),
                                             "DeltaY", DoubleValue (5.0),
                                             "GridWidth", UintegerValue (3),
                                             "LayoutType", StringValue ("RowFirst"));
              mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
              mobility.Install (nodes);

              // Create packet sink on node 1
              PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 9));
              ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (1));

              // Create UDP traffic source on node 0
              OnOffHelper onOffHelper ("ns3::UdpSocketFactory", Address (InetSocketAddress (Ipv4Address ("127.0.0.1"), 9)));
              onOffHelper.SetConstantRate (DataRate ("50Mbps"));
              onOffHelper.SetAttribute ("PacketSize", UintegerValue (packetSize));

              ApplicationContainer app = onOffHelper.Install (nodes.Get (0));
              app.Start (Seconds (0.0));
              app.Stop (Seconds (simulationTime));
              sinkApp.Start (Seconds (0.0));
              sinkApp.Stop (Seconds (simulationTime));

              Simulator::Stop (Seconds (simulationTime));

              uint64_t totalPacketsSent = 0;
              uint64_t totalPacketsReceived = 0;

              Ptr<PacketSink> sink = DynamicCast<PacketSink> (sinkApp.Get (0));

              Simulator::Run ();

              totalPacketsReceived = sink->GetTotalRx ();
              totalPacketsSent = (uint64_t) (simulationTime * 50000000 / (packetSize * 8));

              Simulator::Destroy ();

              double successRate = (double) totalPacketsReceived / totalPacketsSent;
              output << snr << " " << successRate << std::endl;
            }
          output.close ();

          std::string plotFileName = modelName + "-MCS" + std::to_string(mcs) + ".plt";
          std::ofstream plotFile(plotFileName.c_str());
          plotFile << "set terminal png size 640,480" << std::endl;
          plotFile << "set output '" << modelName + "-MCS" + std::to_string(mcs) << ".png'" << std::endl;
          plotFile << "set title '" << modelName << " - MCS " << (int)mcs << "'" << std::endl;
          plotFile << "set xlabel 'SNR (dB)'" << std::endl;
          plotFile << "set ylabel 'Frame Success Rate'" << std::endl;
          plotFile << "plot '" << modelName + "-MCS" + std::to_string(mcs) << ".dat' using 1:2 with linespoints title 'Frame Success Rate'" << std::endl;
          plotFile.close();

          std::string command = "gnuplot " + plotFileName;
          system (command.c_str ());
        }
    }

  return 0;
}