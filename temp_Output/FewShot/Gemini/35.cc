#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OfdmErrorRateModels");

int main(int argc, char *argv[]) {
  bool verbose = false;
  uint32_t frameSize = 1000;
  double simulationTime = 10.0;
  std::string errorRateModelType = "NistErrorRateModel";

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if set to true", verbose);
  cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("errorRateModelType", "Error rate model type (NistErrorRateModel, YansErrorRateModel, TableBasedErrorRateModel)", errorRateModelType);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("OfdmErrorRateModels", LOG_LEVEL_INFO);
  }

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211a);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.Set("RxGain", DoubleValue(10));
  wifiPhy.Set("TxGain", DoubleValue(10));

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");

  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

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

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  UdpClientHelper client(interfaces.GetAddress(1), 9);
  client.SetAttribute("MaxPackets", UintegerValue(1000000));
  client.SetAttribute("Interval", TimeValue(Seconds(0.00001)));
  client.SetAttribute("PacketSize", UintegerValue(frameSize));

  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(simulationTime));

  UdpServerHelper server(9);
  ApplicationContainer serverApps = server.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simulationTime));

  std::map<std::string, std::vector<std::pair<double, double>>> results;

  std::vector<WifiMode> ofdmModes = {
    WifiMode("OfdmRate6Mbps"), WifiMode("OfdmRate9Mbps"), WifiMode("OfdmRate12Mbps"),
    WifiMode("OfdmRate18Mbps"), WifiMode("OfdmRate24Mbps"), WifiMode("OfdmRate36Mbps"),
    WifiMode("OfdmRate48Mbps"), WifiMode("OfdmRate54Mbps")
  };

  for (const WifiMode& wifiMode : ofdmModes) {
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");
    wifi.SetStandard(WIFI_PHY_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue(wifiMode),
                                "ControlMode", StringValue("OfdmRate6Mbps"));

    for (double snr = -5; snr <= 30; snr += 5) {
      double noisePower = -100;
      double txPower = noisePower + snr;

      wifiPhy.Set("TxPowerStart", DoubleValue(txPower));
      wifiPhy.Set("TxPowerEnd", DoubleValue(txPower));

      if (errorRateModelType == "NistErrorRateModel") {
        wifiPhy.SetErrorRateModel("ns3::NistErrorRateModel");
      } else if (errorRateModelType == "YansErrorRateModel") {
        wifiPhy.SetErrorRateModel("ns3::YansErrorRateModel");
      } else if (errorRateModelType == "TableBasedErrorRateModel") {
        wifiPhy.SetErrorRateModel("ns3::TableBasedErrorRateModel",
                                 "FileName", StringValue("src/wifi/model/tables/ieee-80211a.dat"));
      } else {
        std::cerr << "Invalid error rate model type: " << errorRateModelType << std::endl;
        return 1;
      }

      Simulator::Stop(Seconds(simulationTime));
      Simulator::Run();

      uint64_t totalPacketsSent = DynamicCast<UdpClient>(clientApps.Get(0))->GetTotalPacketsSent();
      uint64_t totalPacketsReceived = DynamicCast<UdpServer>(serverApps.Get(0))->GetTotalPacketsReceived();
      double frameSuccessRate = (double)totalPacketsReceived / (double)totalPacketsSent;

      results[wifiMode.GetName()].push_back(std::make_pair(snr, frameSuccessRate));
      NS_LOG_INFO("Mode: " << wifiMode.GetName() << ", SNR: " << snr << ", FSR: " << frameSuccessRate);

      Simulator::Destroy();
    }
  }

  Gnuplot gnuplot("fsr_vs_snr.png");
  gnuplot.SetTitle("Frame Success Rate vs. SNR (" + errorRateModelType + ")");
  gnuplot.SetLegend("SNR (dB)", "Frame Success Rate");

  for (const auto& modeResults : results) {
    Gnuplot2dDataset dataset;
    dataset.SetTitle(modeResults.first);
    dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);
    for (const auto& point : modeResults.second) {
      dataset.Add(point.first, point.second);
    }
    gnuplot.AddDataset(dataset);
  }

  gnuplot.GenerateOutput();

  return 0;
}