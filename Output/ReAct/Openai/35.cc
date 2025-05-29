#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/nist-error-rate-model.h"
#include "ns3/yans-error-rate-model.h"
#include "ns3/table-based-error-rate-model.h"
#include "ns3/wifi-phy.h"
#include "ns3/wifi-phy-standard.h"
#include "ns3/wifi-helper.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <string>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OfdmFsrValidation");

struct OfdmModeEntry
{
  WifiMode mode;
  std::string name;
};

std::vector<OfdmModeEntry> GetOfdmModes()
{
  std::vector<OfdmModeEntry> modes;
  // Standard: 802.11a, OFDM
  WifiPhyHelper phy = WifiPhyHelper::Default ();
  YansWifiPhyHelper yansPhy;
  yansPhy.SetChannel(YansWifiChannelHelper::Default().Create());
  phy.SetChannel(yansPhy.GetChannel());
  phy.Set ("Standard", StringValue ("802.11a"));
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211a);
  Ptr<WifiPhy> phyPtr = phy.Create<WifiPhy> ();
  phyPtr->ConfigureStandard (WIFI_PHY_STANDARD_80211a);

  // These correspond to: 6, 9, 12, 18, 24, 36, 48, 54 Mbps
  std::vector<std::string> ofdmRates = {
    "OfdmRate6Mbps", "OfdmRate9Mbps", "OfdmRate12Mbps", "OfdmRate18Mbps",
    "OfdmRate24Mbps", "OfdmRate36Mbps", "OfdmRate48Mbps", "OfdmRate54Mbps"
  };
  for (const auto& rate : ofdmRates)
    {
      modes.push_back(
        { WifiPhy::GetOFDMRate(rate), rate });
    }
  return modes;
}

// Returns frame success rate for a given error model, SNR (in dB), wifi mode, frameSize (bytes)
double GetFrameSuccessRate(Ptr<ErrorRateModel> errorModel, double snrDb, WifiMode mode, uint32_t frameSize)
{
  double snrLinear = std::pow(10.0, snrDb/10.0);

  // Error models expect a Ptr<WifiPhy> with suitable config, but most BitErrorRate/PacketSuccessRate methods are static
  // We'll use WifiPhy's "GetChunkSuccessRate" function emulated here
  double per = 1.0 - errorModel->GetChunkSuccessRate(mode, snrLinear, frameSize * 8); // bits
  double fsr = 1.0 - per;
  if (fsr < 0.0)
    fsr = 0.0;
  if (fsr > 1.0)
    fsr = 1.0;
  return fsr;
}

int main (int argc, char *argv[])
{
  uint32_t frameSize = 1500; // bytes
  std::string outputPrefix = "ofdm-fsr";

  CommandLine cmd;
  cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
  cmd.AddValue("outputPrefix", "Output filename prefix", outputPrefix);
  cmd.Parse(argc, argv);

  std::vector<OfdmModeEntry> modes = GetOfdmModes();

  // Prepare SNR sweep
  double snrStart = -5.0;
  double snrEnd = 30.0;
  double snrStep = 1.0;

  // Prepare Gnuplot objects for each error model
  Gnuplot nistPlot(outputPrefix + "-nist.png");
  nistPlot.SetTitle("Frame Success Rate vs SNR (NIST Error Model)");
  nistPlot.SetLegend("SNR (dB)", "Frame Success Rate");

  Gnuplot yansPlot(outputPrefix + "-yans.png");
  yansPlot.SetTitle("Frame Success Rate vs SNR (YANS Error Model)");
  yansPlot.SetLegend("SNR (dB)", "Frame Success Rate");

  Gnuplot tablePlot(outputPrefix + "-table.png");
  tablePlot.SetTitle("Frame Success Rate vs SNR (Table-based Error Model)");
  tablePlot.SetLegend("SNR (dB)", "Frame Success Rate");

  // Instantiate error models
  Ptr<NistErrorRateModel> nistModel = CreateObject<NistErrorRateModel> ();
  Ptr<YansErrorRateModel> yansModel = CreateObject<YansErrorRateModel> ();
  Ptr<TableBasedErrorRateModel> tableModel = CreateObject<TableBasedErrorRateModel> ();

  // For Table-based, set a default simple lookup (simulate as NIST)
  tableModel->LoadNistModel(); // supported directly since ns-3.35

  // Iterate modes and SNR, collect FSR data
  for (const auto& entry : modes)
    {
      Gnuplot2dDataset nistDataset(entry.name);
      nistDataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);
      Gnuplot2dDataset yansDataset(entry.name);
      yansDataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);
      Gnuplot2dDataset tableDataset(entry.name);
      tableDataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

      for (double snr = snrStart; snr <= snrEnd + 1e-6; snr += snrStep)
        {
          double nistFsr = GetFrameSuccessRate(nistModel, snr, entry.mode, frameSize);
          double yansFsr = GetFrameSuccessRate(yansModel, snr, entry.mode, frameSize);
          double tableFsr = GetFrameSuccessRate(tableModel, snr, entry.mode, frameSize);

          nistDataset.Add(snr, nistFsr);
          yansDataset.Add(snr, yansFsr);
          tableDataset.Add(snr, tableFsr);
        }
      nistPlot.AddDataset(nistDataset);
      yansPlot.AddDataset(yansDataset);
      tablePlot.AddDataset(tableDataset);
    }

  // Output
  std::ofstream nistF (outputPrefix + "-nist.plt");
  nistPlot.GenerateOutput(nistF);
  nistF.close();

  std::ofstream yansF (outputPrefix + "-yans.plt");
  yansPlot.GenerateOutput(yansF);
  yansF.close();

  std::ofstream tableF (outputPrefix + "-table.plt");
  tablePlot.GenerateOutput(tableF);
  tableF.close();

  std::cout << "Output written to: "
            << outputPrefix << "-nist.plt, "
            << outputPrefix << "-yans.plt, "
            << outputPrefix << "-table.plt" << std::endl;

  return 0;
}