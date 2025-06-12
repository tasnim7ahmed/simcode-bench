#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <map>
#include <string>
#include <iomanip>

using namespace ns3;

struct DsssModeSpec
{
  WifiMode mode;
  std::string label;
};

int main(int argc, char *argv[])
{
  uint32_t frameSize = 1000; // Frame size in bytes
  std::string outputFileName = "dsss-frame-success-rate.eps";

  CommandLine cmd;
  cmd.AddValue ("frameSize", "Frame size in bytes", frameSize);
  cmd.AddValue ("outputFileName", "Output filename for Gnuplot (EPS format)", outputFileName);
  cmd.Parse(argc, argv);

  uint32_t bitSize = frameSize * 8;

  Gnuplot plot(outputFileName);
  plot.SetTitle("Frame Success Rate vs SNR for DSSS Modes");
  plot.SetLegend("SNR (dB)", "Frame Success Rate");
  plot.SetTerminal("postscript eps color");
  plot.AppendExtra("set yrange [0:1]");
  plot.AppendExtra("set grid");

  // DSSS Modes
  std::vector<DsssModeSpec> dsssModes;

  dsssModes.push_back({WifiPhyHelper::Default().GetPhyStandardWifiMode(WIFI_PHY_STANDARD_80211b, 0), "1 Mbps (DSSS)"});
  dsssModes.push_back({WifiPhyHelper::Default().GetPhyStandardWifiMode(WIFI_PHY_STANDARD_80211b, 1), "2 Mbps (DSSS/DBPSK)"});
  dsssModes.push_back({WifiPhyHelper::Default().GetPhyStandardWifiMode(WIFI_PHY_STANDARD_80211b, 2), "5.5 Mbps (DSSS/QPSK)"});
  dsssModes.push_back({WifiPhyHelper::Default().GetPhyStandardWifiMode(WIFI_PHY_STANDARD_80211b, 3), "11 Mbps (DSSS/CCK)"});

  // Error Rate Models
  Ptr<YansErrorRateModel> yansModel = CreateObject<YansErrorRateModel> ();
  Ptr<NistErrorRateModel> nistModel = CreateObject<NistErrorRateModel> ();
  Ptr<TableBasedErrorRateModel> tableModel = CreateObject<TableBasedErrorRateModel> ();

  std::vector<std::pair<ErrorRateModel *, std::string>> models;
  models.push_back({PeekPointer(yansModel), "Yans"});
  models.push_back({PeekPointer(nistModel), "Nist"});
  models.push_back({PeekPointer(tableModel), "TableBased"});

  std::vector<double> snrDbList;
  for (double snr = 0; snr <= 20; snr += 0.5)
  {
    snrDbList.push_back(snr);
  }

  for (const auto& model_pair : models)
  {
    for (const auto& dsss : dsssModes)
    {
      Gnuplot2dDataset dataset;
      std::ostringstream legend;
      legend << dsss.label << " - " << model_pair.second;
      dataset.SetTitle(legend.str());
      dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

      for (const auto& snrDb : snrDbList)
      {
        double snrLinear = std::pow(10.0, snrDb / 10.0);

        double ps = model_pair.first->GetChunkSuccessRate(dsss.mode, snrLinear, static_cast<uint32_t>(bitSize));
        if (ps < 0) ps = 0; // Clamp for safety
        if (ps > 1) ps = 1;

        dataset.Add(snrDb, ps);
      }
      plot.AddDataset(dataset);
    }
  }

  // Expected: frame success rates increase as SNR increases, cross-validate for monotonicity
  for (const auto& model_pair : models)
  {
    for (const auto& dsss : dsssModes)
    {
      double prevPs = 0;
      for (const auto& snrDb : snrDbList)
      {
        double snrLinear = std::pow(10.0, snrDb / 10.0);
        double ps = model_pair.first->GetChunkSuccessRate(dsss.mode, snrLinear, static_cast<uint32_t>(bitSize));
        if (ps < prevPs)
        {
          NS_ABORT_MSG("Non-monotonic Frame Success Rate detected for model=" << model_pair.second << " mode=" << dsss.label << " at SNR=" << snrDb << "dB.");
        }
        prevPs = ps;
      }
    }
  }

  std::ofstream plotFile(outputFileName);
  plot.GenerateOutput(plotFile);
  plotFile.close();

  std::cout << "Output plot written to: " << outputFileName << std::endl;
  return 0;
}