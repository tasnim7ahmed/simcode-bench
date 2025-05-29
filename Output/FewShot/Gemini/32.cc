#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/command-line.h"
#include "ns3/gnuplot.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

int main(int argc, char *argv[]) {
  uint32_t frameSize = 12000;
  double startSnr = 0.0;
  double stopSnr = 25.0;
  double stepSnr = 1.0;
  std::string prefix = "he-error-rate-validation";

  CommandLine cmd;
  cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
  cmd.AddValue("startSnr", "Start SNR in dB", startSnr);
  cmd.AddValue("stopSnr", "Stop SNR in dB", stopSnr);
  cmd.AddValue("stepSnr", "Step SNR in dB", stepSnr);
  cmd.AddValue("prefix", "Output trace file prefix", prefix);
  cmd.Parse(argc, argv);

  std::vector<std::string> errorRateModelNames = {"NistErrorRateModel", "YansErrorRateModel", "TableBasedErrorRateModel"};
  std::vector<WifiHeMcs> mcsValues = {WIFI_HE_MCS0, WIFI_HE_MCS1, WIFI_HE_MCS2, WIFI_HE_MCS3, WIFI_HE_MCS4, WIFI_HE_MCS5, WIFI_HE_MCS6, WIFI_HE_MCS7, WIFI_HE_MCS8, WIFI_HE_MCS9, WIFI_HE_MCS10, WIFI_HE_MCS11};

  for (const auto& modelName : errorRateModelNames) {
    for (const auto& mcs : mcsValues) {
      std::string dataFilename = prefix + "-" + modelName + "-mcs" + std::to_string(mcs) + ".dat";
      std::ofstream dataFile(dataFilename);
      dataFile << "# SNR FrameSuccessRate" << std::endl;

      for (double snr = startSnr; snr <= stopSnr; snr += stepSnr) {
        // Create a single node and set up the PHY
        NodeContainer node;
        node.Create(1);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211_BE);

        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.Set("ChannelType", StringValue("YansWifiChannel"));
        phy.Set("ErrorRateModel", StringValue(modelName));
        phy.Set("TxPowerStart", DoubleValue(snr));
        phy.Set("TxPowerEnd", DoubleValue(snr));
        phy.Set("TxGain", DoubleValue(0));
        phy.Set("RxGain", DoubleValue(0));
        phy.Set("RxNoiseFigure", DoubleValue(0));

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        mac.SetType("AdhocWifiMac");

        NetDeviceContainer device = wifi.Install(phy, mac, node);

        // Calculate the frame success rate
        Ptr<WifiNetDevice> wifiNetDevice = StaticCast<WifiNetDevice>(device.Get(0));
        WifiTxVector txVector;
        txVector.SetHeMcs(mcs);
        double frameSuccessRate = wifiNetDevice->GetFrameSuccessRate(frameSize, txVector, snr);

        dataFile << snr << " " << frameSuccessRate << std::endl;

        Simulator::Destroy();
      }
      dataFile.close();

      // Generate Gnuplot script
      std::string plotFilename = prefix + "-" + modelName + "-mcs" + std::to_string(mcs) + ".plt";
      std::ofstream plotFile(plotFilename);
      plotFile << "set terminal png size 1024,768" << std::endl;
      plotFile << "set output \"" << prefix + "-" + modelName + "-mcs" + std::to_string(mcs) + ".png\"" << std::endl;
      plotFile << "set title \"" << modelName << " - MCS " << std::to_string(mcs) << "\"" << std::endl;
      plotFile << "set xlabel \"SNR (dB)\"" << std::endl;
      plotFile << "set ylabel \"Frame Success Rate\"" << std::endl;
      plotFile << "set grid" << std::endl;
      plotFile << "plot \"" << dataFilename << "\" using 1:2 with lines title \"" << modelName << "\"" << std::endl;
      plotFile.close();

      std::string command = "gnuplot " + plotFilename;
      system(command.c_str());
    }
  }
  return 0;
}