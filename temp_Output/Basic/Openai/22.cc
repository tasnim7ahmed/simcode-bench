#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <string>
#include <iomanip>

using namespace ns3;

static const uint32_t frameSize = 1024; // bytes
static const double snrMin = 0.0;      // dB
static const double snrMax = 20.0;     // dB
static const double snrStep = 0.5;     // dB

struct DsssModeInfo
{
  WifiMode mode;
  std::string label;
};

int main(int argc, char *argv[])
{
  Gnuplot plot("dsss-fsr-vs-snr.eps");
  plot.SetTerminal("postscript eps enhanced color font 'Times-Roman,16'");
  plot.SetTitle("DSSS Frame Success Rate vs SNR");
  plot.SetLegend("SNR (dB)", "Frame Success Rate");
  plot.AppendExtra("set yrange [0:1.05]");
  plot.AppendExtra("set grid");

  std::vector<DsssModeInfo> dsssModes = {
    {WifiPhyHelper::Wifi11b.GetDataMode("DsssRate1Mbps"), "1 Mbps (DsssRate1Mbps)"},
    {WifiPhyHelper::Wifi11b.GetDataMode("DsssRate2Mbps"), "2 Mbps (DsssRate2Mbps)"},
    {WifiPhyHelper::Wifi11b.GetDataMode("DsssRate5_5Mbps"), "5.5 Mbps (DsssRate5_5Mbps)"},
    {WifiPhyHelper::Wifi11b.GetDataMode("DsssRate11Mbps"), "11 Mbps (DsssRate11Mbps)"}
  };

  Ptr<YansErrorRateModel> yans = CreateObject<YansErrorRateModel>();
  Ptr<NistErrorRateModel> nist = CreateObject<NistErrorRateModel>();
  Ptr<TableBasedErrorRateModel> table = CreateObject<TableBasedErrorRateModel>();

  std::vector<std::pair<std::string, Ptr<ErrorRateModel>>> models = {
    {"Yans", yans},
    {"Nist", nist},
    {"Table", table}
  };

  for (const auto& dsss : dsssModes)
    {
      for (const auto& mdl : models)
        {
          std::ostringstream title;
          title << dsss.label << " - " << mdl.first;
          Gnuplot2dDataset dataset(title.str());
          dataset.SetStyle(Gnuplot2dDataset::LINES);

          for (double snr = snrMin; snr <= snrMax + 1e-6; snr += snrStep)
            {
              double snrLinear = std::pow(10.0, snr / 10.0);
              double per = mdl.second->GetChunkSuccessRate(dsss.mode, snrLinear, frameSize * 8);
              double fsr = per;
              if (fsr < 0.0)
                fsr = 0.0;
              if (fsr > 1.0)
                fsr = 1.0;
              dataset.Add(snr, fsr);
            }
          plot.AddDataset(dataset);
        }
    }

  std::ofstream plotFile("dsss-fsr-vs-snr.plt");
  plot.GenerateOutput(plotFile);
  plotFile.close();
  return 0;
}