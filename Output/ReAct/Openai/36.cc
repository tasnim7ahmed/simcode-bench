#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <string>
#include <iomanip>

using namespace ns3;

struct RateEntry
{
  WifiMode mode;
  std::string name;
  uint8_t mcsIndex;
};

std::vector<RateEntry> GetVhtRates()
{
  // 802.11ac VHT MCS 0-9, for 20 MHz, we skip MCS 9 as mandated by the instructions
  std::vector<RateEntry> vhtModes;
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211ac);
  WifiMacHelper mac;
  Ssid ssid = Ssid ("vht");
  NetDeviceContainer devices;
  NodeContainer nodes;
  nodes.Create(2);
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
  phy.SetChannel(YansWifiChannelHelper::Default().Create());
  devices = wifi.Install(phy, mac, nodes);
  Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice>(devices.Get(0));
  Ptr<WifiPhy> wifiPhy = wifiDev->GetPhy();

  // Get VHT rates
  for (uint8_t mcs = 0; mcs <= 8; ++mcs)
    {
      WifiMode mode = WifiPhy::GetVhtMcs(mcs, 1, false); // 1 spatial stream, short GI false
      std::ostringstream oss;
      oss << "MCS " << unsigned(mcs);
      vhtModes.push_back({mode, oss.str(), mcs});
    }
  return vhtModes;
}

Ptr<ErrorRateModel> CreateErrorRateModel(const std::string& modelName)
{
  if (modelName == "nist")
    {
      return CreateObject<NistErrorRateModel>();
    }
  if (modelName == "yans")
    {
      return CreateObject<YansErrorRateModel>();
    }
  if (modelName == "table")
    {
      return CreateObject<TableBasedErrorRateModel>();
    }
  NS_ABORT_MSG("Unknown error model: " + modelName);
  return nullptr;
}

double GetSnrDb(uint32_t snrIndex)
{
  // SNR from -5 dB to 30 dB in 1 dB steps
  return static_cast<double>(snrIndex) - 5.0;
}

int main (int argc, char *argv[])
{
  uint32_t frameSize = 1200;
  CommandLine cmd;
  cmd.AddValue ("frameSize", "Frame size (bytes)", frameSize);
  cmd.Parse (argc, argv);

  std::vector<std::string> errorModels = { "nist", "yans", "table" };
  std::vector<RateEntry> vhtRates = GetVhtRates();

  std::vector<double> snrDbList;
  for (uint32_t i = 0; i <= 35; ++i) // SNR from -5 dB to 30 dB
    {
      snrDbList.push_back(GetSnrDb(i));
    }

  for (const std::string& modelName : errorModels)
    {
      Gnuplot plot;
      plot.SetTitle ("Frame Success Rate vs SNR for " + modelName + " Error Model (VHT rates, " +
                     std::to_string(frameSize) + " bytes)");
      plot.SetLegend ("SNR [dB]", "Frame Success Rate");
      plot.AppendExtra("set xrange [-5:30]");
      plot.AppendExtra("set yrange [0:1]");
      plot.AppendExtra("set key outside");

      for (const RateEntry& rate : vhtRates)
        {
          Gnuplot2dDataset dataset;
          dataset.SetTitle(rate.name);
          dataset.SetStyle(Gnuplot2dDataset::LINES);

          Ptr<ErrorRateModel> errModel = CreateErrorRateModel(modelName);

          for (double snrDb : snrDbList)
            {
              double snrLinear = std::pow(10.0, snrDb/10.0);

              double per = errModel->GetChunkSuccessRate(rate.mode, frameSize * 8, snrLinear);
              dataset.Add (snrDb, per);
            }
          plot.AddDataset (dataset);
        }
      std::ostringstream fname;
      fname << "vht-fsr-" << modelName << "-frame" << frameSize << ".plt";
      std::ofstream plotFile (fname.str());
      plot.GenerateOutput (plotFile);
      plotFile.close ();
    }
  return 0;
}