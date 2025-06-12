#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/gnuplot.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>

using namespace ns3;

int main(int argc, char *argv[]) {
  bool verbose = false;
  uint32_t packetSize = 1472;
  double simulationTime = 1.0;
  std::string phyMode("HtMcs7");
  double minSnr = 5.0;
  double maxSnr = 30.0;
  double snrStep = 1.0;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if set to true", verbose);
  cmd.AddValue("packetSize", "Size of packet generated. Default: 1472", packetSize);
  cmd.AddValue("simulationTime", "Simulation time in seconds. Default: 1", simulationTime);
  cmd.AddValue("phyMode", "Wifi Phy mode. Default: HtMcs7", phyMode);
  cmd.AddValue("minSnr", "Minimum SNR value (dB). Default: 5.0", minSnr);
  cmd.AddValue("maxSnr", "Maximum SNR value (dB). Default: 30.0", maxSnr);
  cmd.AddValue("snrStep", "SNR step size (dB). Default: 1.0", snrStep);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("ErrorRateComparison", LOG_LEVEL_INFO);
  }

  std::vector<std::string> models = {"NistErrorRateModel", "YansErrorRateModel", "TableBasedErrorRateModel"};
  std::map<std::string, std::ofstream> outputFiles;
  std::map<std::string, Gnuplot2dDataset> datasets;
  std::map<std::string, std::string> dataFileNames;

  for (const auto& model : models) {
    std::string fileName = "error_rate_" + model + "_" + phyMode + ".dat";
    outputFiles[model] = std::ofstream(fileName);
    dataFileNames[model] = fileName;
    datasets[model].SetStyle(Gnuplot2dDataset::LINES);
    datasets[model].SetTitle(model);
  }

  for (double snr = minSnr; snr <= maxSnr; snr += snrStep) {
    for (const auto& model : models) {
      NodeContainer nodes;
      nodes.Create(2);

      WifiHelper wifi;
      wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

      YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
      wifiPhy.Set("RxGain", DoubleValue(0));
      wifiPhy.Set("TxGain", DoubleValue(0));

      Config::SetDefault("ns3::YansWifiPhy::ChannelNumber", UintegerValue(1));
      Config::SetDefault("ns3::YansWifiPhy::ShortGuardEnabled", BooleanValue(true));

      YansWifiChannelHelper wifiChannel;
      wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
      wifiChannel.AddPropagationLoss("ns3::FixedRssLossModel", "Rss", DoubleValue(-snr));
      Ptr<YansWifiChannel> channel = wifiChannel.Create();
      wifiPhy.SetChannel(channel);

      WifiMacHelper wifiMac;
      wifiMac.SetType("ns3::AdhocWifiMac");
      NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

      // Configure error rate model
      Config::SetDefault("ns3::WifiNetDevice::DataMode", StringValue(phyMode));
      Config::SetDefault("ns3::WifiNetDevice::ControlMode", StringValue("OfdmRate6Mbps"));
      if (model == "NistErrorRateModel") {
        Config::SetDefault("ns3::WifiPhy::ErrorRateModel", StringValue("ns3::NistErrorRateModel"));
      } else if (model == "YansErrorRateModel") {
        Config::SetDefault("ns3::WifiPhy::ErrorRateModel", StringValue("ns3::YansErrorRateModel"));
      } else if (model == "TableBasedErrorRateModel") {
         Config::SetDefault("ns3::WifiPhy::ErrorRateModel", StringValue("ns3::TableBasedErrorRateModel"));
      }

      // Set mobility model
      MobilityHelper mobility;
      mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue(0.0),
                                     "MinY", DoubleValue(0.0),
                                     "DeltaX", DoubleValue(5.0),
                                     "DeltaY", DoubleValue(10.0),
                                     "GridWidth", UintegerValue(3),
                                     "LayoutType", StringValue("RowFirst"));
      mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
      mobility.Install(nodes);

      // Create traffic
      TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
      Ptr<Socket> recvSink = Socket::CreateSocket(nodes.Get(1), tid);
      InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 8080);
      recvSink->Bind(local);

      Ptr<Socket> source = Socket::CreateSocket(nodes.Get(0), tid);
      InetSocketAddress remote = InetSocketAddress(Ipv4Address("127.0.0.1"), 8080); // Use loopback for simplicity
      source->Connect(remote);
      source->SetAllowBroadcast(true);

      Ptr<Packet> packet = Create<Packet>(packetSize);

      Simulator::ScheduleWithContext(source->GetNode()->GetId(), Seconds(0.001), [source, packet]() {
          source->Send(packet);
      });

      uint32_t totalTxPackets = 0;
      uint32_t totalRxPackets = 0;

      Simulator::Schedule(Seconds(simulationTime), [&totalTxPackets, &totalRxPackets, source, recvSink]() {
        totalTxPackets = source->GetTxPackets();
        totalRxPackets = recvSink->GetRxPackets();
      });

      Simulator::Run(Seconds(simulationTime));
      Simulator::Destroy();

      double successRate = (double)totalRxPackets / (double)totalTxPackets;
      outputFiles[model] << snr << " " << successRate << std::endl;
      datasets[model].Add(snr, successRate);
    }
  }

  Gnuplot gnuplot("error_rate_comparison_" + phyMode + ".png");
  gnuplot.SetTitle("Frame Success Rate vs SNR (" + phyMode + ")");
  gnuplot.SetLegend("Error Rate Model", "Frame Success Rate");

  for (const auto& model : models) {
    gnuplot.AddDataset(datasets[model]);
    outputFiles[model].close();
  }

  gnuplot.GenerateOutput(std::cout);
  return 0;
}