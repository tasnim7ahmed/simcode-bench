#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/yans-error-rate-model.h"
#include "ns3/nist-error-rate-model.h"
#include "ns3/table-error-rate-model.h"
#include "ns3/gnuplot.h"
#include "ns3/dsss-phy.h"

#include <vector>
#include <cmath>
#include <map>

using namespace ns3;

struct DsssModeInfo
{
  std::string name;
  WifiMode mode;
  double bitRateMbps;
  uint32_t spreading;
  double minSnrForSuccess;
  double maxSnrForFailure;
};

static std::vector<DsssModeInfo>
GetDsssModes()
{
  std::vector<DsssModeInfo> modes;
  WifiPhyHelper phyHelper = WifiPhyHelper::Default();
  Ptr<DsssPhy> dsssPhy = CreateObject<DsssPhy>();
  // DSSS modes in 802.11b (1, 2, 5.5, 11 Mbps)  
  modes.push_back({"DSSS_1Mbps", WifiPhy::GetDsssRate1Mbps(), 1.0, 11, 6.0, 2.0});
  modes.push_back({"DSSS_2Mbps", WifiPhy::GetDsssRate2Mbps(), 2.0, 11, 8.0, 4.0});
  modes.push_back({"DSSS_5_5Mbps", WifiPhy::GetDsssRate5_5Mbps(), 5.5, 8, 10.0, 6.0});
  modes.push_back({"DSSS_11Mbps", WifiPhy::GetDsssRate11Mbps(), 11.0, 8, 14.0, 8.0});
  return modes;
}

static double
DbToRatio(double db)
{
  return std::pow(10.0, db/10.0);
}

int
main(int argc, char *argv[])
{
  uint32_t frameSizeBytes = 1500;
  CommandLine cmd;
  cmd.AddValue("frameSize", "Frame size (bytes)", frameSizeBytes);
  cmd.Parse(argc, argv);

  const uint32_t frameSizeBits = frameSizeBytes * 8;
  std::vector<DsssModeInfo> dsssModes = GetDsssModes();
  std::vector<std::string> errorModels = {"Yans", "Nist", "Table"};
  std::map<std::string, Ptr<ErrorRateModel>> modelPtrs;
  modelPtrs["Yans"] = CreateObject<YansErrorRateModel>();
  modelPtrs["Nist"] = CreateObject<NistErrorRateModel>();
  modelPtrs["Table"] = CreateObject<TableErrorRateModel>();

  // SNR values from 0 dB to 20 dB in steps of 1 dB
  std::vector<double> snrDbVec;
  for (double snr = 0.0; snr <= 20.0; snr += 1.0)
  {
    snrDbVec.push_back(snr);
  }

  // Gnuplot setup
  Gnuplot plot("dsss-fsr-snr.eps");
  plot.SetTitle("DSSS Frame Success Rate vs SNR");
  plot.SetTerminal("postscript eps enhanced color font 'Helvetica,18'");
  plot.SetLegend("SNR (dB)", "Frame Success Rate");
  plot.AppendExtra("set grid");
  plot.AppendExtra("set key left top");

  for (auto &&modeInfo : dsssModes)
  {
    for (auto &&modelName : errorModels)
    {
      Gnuplot2dDataset dataset;
      std::ostringstream label;
      label << modeInfo.name << "-" << modelName;
      dataset.SetTitle(label.str());
      dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

      for (auto &&snrDb : snrDbVec)
      {
        double snr = DbToRatio(snrDb);

        Ptr<ErrorRateModel> model = modelPtrs[modelName];

        // Calculate frame error rate
        double per = model->GetChunkSuccessRate(modeInfo.mode, snr, frameSizeBits);
        double fsr = per;

        // Frame Success Rate (not error rate)
        // Depending on NS-3 error model definition, if GetChunkSuccessRate returns P(success) or P(error):
        // ns-3 returns P(success), so fsr = per.

        // Validate FSR is between 0 and 1
        if (fsr < 0.0) fsr = 0.0;
        if (fsr > 1.0) fsr = 1.0;

        dataset.Add(snrDb, fsr);
      }
      plot.AddDataset(dataset);
    }
  }

  // Output plot to file
  std::ofstream plotFile("dsss-fsr-snr.plt");
  plot.GenerateOutput(plotFile);
  plotFile.close();

  return 0;
}