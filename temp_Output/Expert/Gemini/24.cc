#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include "ns3/ssid.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  bool enableNist = true;
  bool enableYans = true;
  bool enableTable = true;
  uint32_t frameSize = 1000;
  uint32_t minMCS = 0;
  uint32_t maxMCS = 7;
  double snrStep = 1.0;
  double minSNR = 5.0;
  double maxSNR = 25.0;

  CommandLine cmd;
  cmd.AddValue("enableNist", "Enable Nist error model", enableNist);
  cmd.AddValue("enableYans", "Enable Yans error model", enableYans);
  cmd.AddValue("enableTable", "Enable Table error model", enableTable);
  cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
  cmd.AddValue("minMCS", "Minimum MCS value", minMCS);
  cmd.AddValue("maxMCS", "Maximum MCS value", maxMCS);
  cmd.AddValue("snrStep", "SNR step value", snrStep);
  cmd.AddValue("minSNR", "Minimum SNR value", minSNR);
  cmd.AddValue("maxSNR", "Maximum SNR value", maxSNR);
  cmd.Parse(argc, argv);

  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  std::vector<double> snrValues;
  for (double snr = minSNR; snr <= maxSNR; snr += snrStep) {
    snrValues.push_back(snr);
  }

  std::vector<uint32_t> mcsValues;
  for (uint32_t mcs = minMCS; mcs <= maxMCS; ++mcs) {
    mcsValues.push_back(mcs);
  }

  std::map<std::string, std::vector<double>> ferData;

  for (uint32_t mcs : mcsValues) {
    for (double snr : snrValues) {
      for (int model = 0; model < 3; ++model) {
        std::string modelName;
        if (model == 0) {
          if (!enableNist) continue;
          modelName = "Nist";
        } else if (model == 1) {
          if (!enableYans) continue;
          modelName = "Yans";
        } else {
          if (!enableTable) continue;
          modelName = "Table";
        }

        NodeContainer nodes;
        nodes.Create(2);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        YansPropDelayModel delayModel;
        wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
        wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
        wifiPhy.SetChannel(wifiChannel.Create());
        wifiPhy.Set("TxPowerStart", DoubleValue(16));
        wifiPhy.Set("TxPowerEnd", DoubleValue(16));
        wifiPhy.Set("TxPowerLevels", UintegerValue(1));
        wifiPhy.Set("AntennasAtTransmitter", UintegerValue(1));
        wifiPhy.Set("AntennasAtReceiver", UintegerValue(1));

        if (model == 0) {
          NistErrorRateModel errorModel;
          errorModel.Set("SNR", DoubleValue(snr));
          wifiPhy.SetErrorRateModel("ns3::NistErrorRateModel", "SNR", DoubleValue(snr));
        } else if (model == 1) {
          YansErrorRateModel errorModel;
          Config::SetDefault("ns3::YansErrorRateModel::Ber", DoubleValue(0.0));
          wifiPhy.SetErrorRateModel("ns3::YansErrorRateModel");
        } else {
          TableBasedErrorRateModel errorModel;
          errorModel.Set("DataRate", StringValue("OfdmRate" + std::to_string(mcs * 2 + 1))); //Map MCS to OFDM rate string
          errorModel.Set("ErrorRateTable", StringValue("src/devices/wifi/utils/errortables/ieee-80211n-ht-mcs.txt"));
          wifiPhy.SetErrorRateModel("ns3::TableBasedErrorRateModel", "DataRate", StringValue("OfdmRate" + std::to_string(mcs * 2 + 1)), "ErrorRateTable", StringValue("src/devices/wifi/utils/errortables/ieee-80211n-ht-mcs.txt"));
        }

        WifiMacHelper wifiMac;
        Ssid ssid = Ssid("ns3-wifi");
        wifiMac.SetType("ns3::StaWifiMac",
                      "Ssid", SsidValue(ssid),
                      "ActiveProbing", BooleanValue(false));

        NetDeviceContainer staDevices;
        staDevices = wifi.Install(wifiPhy, wifiMac, nodes.Get(1));

        wifiMac.SetType("ns3::ApWifiMac",
                      "Ssid", SsidValue(ssid),
                      "BeaconInterval", TimeValue(Seconds(0.1)));

        NetDeviceContainer apDevices;
        apDevices = wifi.Install(wifiPhy, wifiMac, nodes.Get(0));

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

        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(apDevices);
        address.Assign(staDevices);

        UdpServerHelper server(9);
        ApplicationContainer serverApps = server.Install(nodes.Get(0));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        UdpClientHelper client(interfaces.GetAddress(0), 9);
        client.SetAttribute("MaxPackets", UintegerValue(1000));
        client.SetAttribute("Interval", TimeValue(Time("ns", 1)));
        client.SetAttribute("PacketSize", UintegerValue(frameSize));

        ApplicationContainer clientApps = client.Install(nodes.Get(1));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

        Simulator::Stop(Seconds(10.0));
        Simulator::Run();

        Ptr<UdpServer> serverApp = DynamicCast<UdpServer>(serverApps.Get(0));
        double packetLossRatio = 1.0 - (double)serverApp->GetReceived() / 1000.0; //Approximate FER
        double fer = packetLossRatio;

        std::string key = modelName + "-MCS" + std::to_string(mcs);
        if (ferData.find(key) == ferData.end()) {
          ferData[key] = std::vector<double>();
        }
        ferData[key].push_back(fer);
        Simulator::Destroy();
      }
    }
  }

  Gnuplot2dDataset dataset;
  Gnuplot plot("fer_vs_snr.eps");
  plot.SetTitle("FER vs. SNR for Different Error Models and MCS");
  plot.SetLegend ("SNR", "FER");
  for (auto const& [key, val] : ferData) {
      dataset.SetTitle(key);
      dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);
      int i = 0;
      for (double snr : snrValues) {
          dataset.Add(snr, val[i]);
          ++i;
      }
      plot.AddDataset(dataset);
      dataset.Clear();
  }
  plot.GenerateOutput();

  return 0;
}