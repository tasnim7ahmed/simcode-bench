#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiErrorRateComparison");

int main(int argc, char *argv[]) {

  // Enable logging
  LogComponentEnable("WifiErrorRateComparison", LOG_LEVEL_INFO);

  // Set default values for simulation parameters
  uint32_t frameSize = 1000;
  uint32_t numPackets = 100;
  double simulationTime = 10.0;
  double startSnr = 5.0;
  double endSnr = 25.0;
  double snrStep = 2.0;
  uint32_t startMcs = 0;
  uint32_t endMcs = 7;
  bool useNist = true;
  bool useYans = true;
  bool useTable = true;

  // Command line arguments parsing
  CommandLine cmd;
  cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
  cmd.AddValue("numPackets", "Number of packets to send per SNR", numPackets);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("startSnr", "Start SNR value", startSnr);
  cmd.AddValue("endSnr", "End SNR value", endSnr);
  cmd.AddValue("snrStep", "SNR step value", snrStep);
  cmd.AddValue("startMcs", "Start MCS value", startMcs);
  cmd.AddValue("endMcs", "End MCS value", endMcs);
  cmd.AddValue("useNist", "Use Nist error model", useNist);
  cmd.AddValue("useYans", "Use Yans error model", useYans);
  cmd.AddValue("useTable", "Use Table error model", useTable);
  cmd.Parse(argc, argv);

  // Create Gnuplot object
  Gnuplot gnuplot("wifi_error_rate_comparison.eps");
  gnuplot.SetTitle("FER vs. SNR for different MCS and Error Models");
  gnuplot.SetLegend("SNR (dB)", "FER");

  // Loop through MCS values
  for (uint32_t mcs = startMcs; mcs <= endMcs; ++mcs) {
    // Create datasets for each error model for the current MCS
    Gnuplot2dDataset nistDataset;
    nistDataset.SetTitle("Nist MCS " + std::to_string(mcs));
    nistDataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    Gnuplot2dDataset yansDataset;
    yansDataset.SetTitle("Yans MCS " + std::to_string(mcs));
    yansDataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    Gnuplot2dDataset tableDataset;
    tableDataset.SetTitle("Table MCS " + std::to_string(mcs));
    tableDataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    // Loop through SNR values
    for (double snr = startSnr; snr <= endSnr; snr += snrStep) {
      // Create simulation environment for each SNR value

      NodeContainer nodes;
      nodes.Create(2);

      WifiHelper wifi;
      wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

      YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
      YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
      wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
      wifiChannel.AddPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
      wifiPhy.SetChannel(wifiChannel.Create());
      wifiPhy.Set("TxPowerStart", DoubleValue(snr));
      wifiPhy.Set("TxPowerEnd", DoubleValue(snr));
      wifiPhy.Set("TxPowerLevels", UintegerValue(1));

      WifiMacHelper wifiMac;
      Ssid ssid = Ssid("ns-3-ssid");
      wifiMac.SetType("ns3::StaWifiMac",
                      "Ssid", SsidValue(ssid),
                      "ActiveProbing", BooleanValue(false));

      NetDeviceContainer staDevices;
      staDevices = wifi.Install(wifiPhy, wifiMac, nodes.Get(1));

      wifiMac.SetType("ns3::ApWifiMac",
                      "Ssid", SsidValue(ssid));

      NetDeviceContainer apDevices;
      apDevices = wifi.Install(wifiPhy, wifiMac, nodes.Get(0));

      // Set MCS
      Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_Qdisc/Txop/Queue/MaxSize", StringValue("1000p"));
      Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_Qdisc/Txop/Queue/MaxDelay", TimeValue(MilliSeconds(100)));

      Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue(20));
      Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/GuardInterval", EnumValue(WifiPhy::GI_800NS));
      Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ShortGuardEnabled", BooleanValue(true));

      // Set error rate model(s)
      if (useNist) {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ErrorRateModel", StringValue("ns3::NistErrorRateModel"));
      } else if (useYans) {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ErrorRateModel", StringValue("ns3::YansErrorRateModel"));
      } else if (useTable) {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ErrorRateModel", StringValue("ns3::TableBasedErrorRateModel"));
      }

      Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/MinstrelHt/UseGreenfield", BooleanValue(false));
      Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/MinstrelHt/MaxAmpduSize", UintegerValue(8191));
      Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/MinstrelHt/EnableRtsCts", BooleanValue(false));
      Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/MinstrelHt/DataMcs", UintegerValue(mcs));

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

      // Install TCP Receiver on the access point
      uint16_t port = 9;
      Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
      PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
      ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(0));
      sinkApps.Start(Seconds(0.0));
      sinkApps.Stop(Seconds(simulationTime + 1));

      // Install TCP Transmitter on the station
      InetSocketAddress remoteAddress(Ipv4Address("127.0.0.1"), port);
      remoteAddress.SetIpv4(apDevices.Get(0)->GetObject<Ipv4>()->GetAddress(0, 0).GetLocal());

      OnOffHelper onOffHelper("ns3::TcpSocketFactory", remoteAddress);
      onOffHelper.SetConstantRate(DataRate("50Mbps"));
      onOffHelper.SetAttribute("PacketSize", UintegerValue(frameSize));
      onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
      onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

      ApplicationContainer clientApps = onOffHelper.Install(nodes.Get(1));
      clientApps.Start(Seconds(1.0));
      clientApps.Stop(Seconds(simulationTime));

      Ipv4AddressHelper address;
      address.SetBase("127.0.0.0", "255.255.255.0");
      Ipv4InterfaceContainer interfaces = address.Assign(staDevices);
      interfaces = address.Assign(apDevices);

      // Run the simulation
      Simulator::Stop(Seconds(simulationTime + 1));
      Simulator::Run();

      // Calculate FER
      Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApps.Get(0));
      uint64_t totalPacketsReceived = sink->GetTotalRx();
      double fer = 1.0 - ((double)totalPacketsReceived / (numPackets * simulationTime * 50e6 / (frameSize * 8)));

      // Add data point to the appropriate dataset
      if (useNist) {
        nistDataset.Add(snr, fer);
      }
      if (useYans) {
        yansDataset.Add(snr, fer);
      }
      if (useTable) {
        tableDataset.Add(snr, fer);
      }

      Simulator::Destroy();
    }

    // Add datasets to the Gnuplot object
    if (useNist) {
      gnuplot.AddDataset(nistDataset);
    }
    if (useYans) {
      gnuplot.AddDataset(yansDataset);
    }
    if (useTable) {
      gnuplot.AddDataset(tableDataset);
    }
  }

  // Generate the plot
  gnuplot.GenerateOutput();

  std::cout << "Simulation finished. Gnuplot output written to wifi_error_rate_comparison.eps" << std::endl;

  return 0;
}