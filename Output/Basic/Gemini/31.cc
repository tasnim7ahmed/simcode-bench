#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include "ns3/config-store.h"
#include "ns3/spectrum-module.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

using namespace ns3;

int main(int argc, char *argv[]) {

  bool enablePcap = false;
  double simDuration = 10.0;
  double distance = 10.0;
  uint32_t packetSize = 1472;
  uint32_t numPackets = 1000;
  double minSnr = 0.0;
  double maxSnr = 30.0;
  double snrStep = 1.0;
  std::string rateAdaptation = "AarfWifiManager";

  CommandLine cmd;
  cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue("simDuration", "Simulation duration in seconds", simDuration);
  cmd.AddValue("distance", "Distance between nodes in meters", distance);
  cmd.AddValue("packetSize", "Size of each packet in bytes", packetSize);
  cmd.AddValue("numPackets", "Number of packets to send", numPackets);
  cmd.AddValue("minSnr", "Minimum SNR value", minSnr);
  cmd.AddValue("maxSnr", "Maximum SNR value", maxSnr);
  cmd.AddValue("snrStep", "SNR step size", snrStep);
  cmd.AddValue("rateAdaptation", "Rate Adaptation Algorithm", rateAdaptation);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue("DsssRate1Mbps"));

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211_EHT);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Create();
  phy.SetChannel(channel.Create());
  phy.Set("ChannelWidth", UintegerValue(20));

  NistErrorRateModelHelper nistErrorRateModel;
  YansErrorRateModelHelper yansErrorRateModel;

  wifi.SetRemoteStationManager(rateAdaptation,
                                 "DataMode", StringValue("HeMcs0"),
                                 "ControlMode", StringValue("HeMcs0"));

  NetDeviceContainer devices = wifi.Install(phy, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(distance),
                                 "DeltaY", DoubleValue(0.0),
                                 "GridWidth", UintegerValue(3),
                                 "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(simDuration));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.001)));
  echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

  std::vector<std::string> mcsValues = {"HeMcs0", "HeMcs1", "HeMcs2", "HeMcs3", "HeMcs4", "HeMcs5", "HeMcs6", "HeMcs7", "HeMcs8", "HeMcs9", "HeMcs10", "HeMcs11"};

  for (const auto& mcs : mcsValues) {
    std::cout << "Running simulations for MCS: " << mcs << std::endl;

    Gnuplot2dDataset datasetNist;
    datasetNist.SetTitle("Nist");
    datasetNist.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    Gnuplot2dDataset datasetYans;
    datasetYans.SetTitle("Yans");
    datasetYans.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    Gnuplot2dDataset datasetTable;
    datasetTable.SetTitle("Table");
    datasetTable.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    for (double snr = minSnr; snr <= maxSnr; snr += snrStep) {
      std::cout << "SNR: " << snr << std::endl;

      phy.Set("RxGain", DoubleValue(snr));

      wifi.SetRemoteStationManager(rateAdaptation,
                                     "DataMode", StringValue(mcs),
                                     "ControlMode", StringValue(mcs));

      phy.SetErrorRateModel("ns3::NistErrorRateModel");
      ApplicationContainer clientAppsNist = echoClient.Install(nodes.Get(0));
      clientAppsNist.Start(Seconds(1.0));
      clientAppsNist.Stop(Seconds(simDuration));
      Simulator::Run();
      Simulator::Stop(Seconds(simDuration + 1));
      uint32_t nistRx = DynamicCast<UdpEchoServer>(serverApps.Get(0))->GetReceived();
      double nistSuccessRate = (double)nistRx / numPackets;
      datasetNist.Add(snr, nistSuccessRate);
      clientAppsNist.Stop(Seconds(0.0));
      Simulator::Destroy();

      phy.SetErrorRateModel("ns3::YansErrorRateModel");
      ApplicationContainer clientAppsYans = echoClient.Install(nodes.Get(0));
      clientAppsYans.Start(Seconds(1.0));
      clientAppsYans.Stop(Seconds(simDuration));
      Simulator::Run();
      Simulator::Stop(Seconds(simDuration + 1));
      uint32_t yansRx = DynamicCast<UdpEchoServer>(serverApps.Get(0))->GetReceived();
      double yansSuccessRate = (double)yansRx / numPackets;
      datasetYans.Add(snr, yansSuccessRate);
      clientAppsYans.Stop(Seconds(0.0));
      Simulator::Destroy();

      phy.SetErrorRateModel("ns3::TableBasedErrorRateModel");
       ApplicationContainer clientAppsTable = echoClient.Install(nodes.Get(0));
      clientAppsTable.Start(Seconds(1.0));
      clientAppsTable.Stop(Seconds(simDuration));
      Simulator::Run();
      Simulator::Stop(Seconds(simDuration + 1));
      uint32_t tableRx = DynamicCast<UdpEchoServer>(serverApps.Get(0))->GetReceived();
      double tableSuccessRate = (double)tableRx / numPackets;
      datasetTable.Add(snr, tableSuccessRate);
      clientAppsTable.Stop(Seconds(0.0));
      Simulator::Destroy();
    }

    std::string filename = "eht_error_rate_mcs_" + mcs + ".png";
    Gnuplot gnuplot(filename);
    gnuplot.SetTitle("Frame Success Rate vs SNR for MCS " + mcs);
    gnuplot.SetLegend("SNR (dB)", "Frame Success Rate");
    gnuplot.AddDataset(datasetNist);
    gnuplot.AddDataset(datasetYans);
    gnuplot.AddDataset(datasetTable);
    gnuplot.GenerateOutput();

  }

  return 0;
}