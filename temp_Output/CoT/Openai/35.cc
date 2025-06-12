#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <string>
#include <iomanip>

using namespace ns3;

struct OfdmModeRecord
{
  std::string modeName;
  WifiMode mode;
};

static std::vector<OfdmModeRecord> GetOfdmModes()
{
  std::vector<OfdmModeRecord> ret;
  WifiPhyHelper phy = WifiPhyHelper::Default();
  WifiHelper wifi;

  // Use 802.11a OFDM rates for this example
  wifi.SetStandard(WIFI_PHY_STANDARD_80211a);
  Ptr<WifiRemoteStationManager> manager = CreateObject<ConstantRateWifiManager>();
  for (uint32_t i = 0; i < phy.GetNModes(); ++i)
    {
      WifiMode mode = phy.GetMode(i);
      std::string mf = mode.GetModulationClass () == WIFI_MOD_CLASS_OFDM
        ? mode.GetUniqueName()
        : "";
      if (mf.size() && mode.GetDataRate(MAC_PHY_LAYER) > 0)
        {
          ret.push_back({mode.GetUniqueName(), mode});
        }
    }
  return ret;
}

static double
DbToRatio(double db)
{
  return std::pow(10.0, db / 10.0);
}

static void
PlotGnuplot(const std::string &filePrefix,
            const std::vector<std::string> &modelNames,
            const std::vector<std::vector<Gnuplot2dDataset>> &datasets,
            const std::vector<OfdmModeRecord> &ofdmModes,
            const std::vector<double> &snrVals)
{
  for (size_t modelIndex = 0; modelIndex < modelNames.size(); modelIndex++)
    {
      Gnuplot plot;
      plot.SetTitle("Frame Success Rate vs. SNR for " + modelNames[modelIndex] + " Model");
      plot.SetLegend("SNR (dB)", "Frame Success Rate");
      for (size_t modeIndex = 0; modeIndex < ofdmModes.size(); modeIndex++)
        {
          plot.AddDataset(datasets[modelIndex][modeIndex]);
        }
      std::string pltFile = filePrefix + "_" + modelNames[modelIndex] + ".plt";
      std::ofstream of(pltFile.c_str());
      plot.GenerateOutput(of);
      of.close();
    }
}

int
main(int argc, char *argv[])
{
  uint32_t frameSize = 1200;
  double minSnrDb = -5.0;
  double maxSnrDb = 30.0;
  double snrStepDb = 1.0;

  CommandLine cmd;
  cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
  cmd.Parse(argc, argv);

  std::vector<OfdmModeRecord> ofdmModes = GetOfdmModes();

  std::vector<std::string> modelNames = {"Nist", "Yans", "Table"};
  std::vector<std::vector<Gnuplot2dDataset>> allDatasets(modelNames.size());

  std::vector<double> snrVals;
  for (double snr = minSnrDb; snr <= maxSnrDb + 1e-6; snr += snrStepDb)
    snrVals.push_back(snr);

  for (size_t modelIndex = 0; modelIndex < modelNames.size(); modelIndex++)
    {
      std::vector<Gnuplot2dDataset> datasets;
      for (const auto &ofdmRec : ofdmModes)
        {
          Gnuplot2dDataset ds;
          ds.SetTitle(ofdmRec.modeName);
          ds.SetStyle(Gnuplot2dDataset::LINES_POINTS);

          // Select error model
          Ptr<ErrorRateModel> errorModel;
          if (modelNames[modelIndex] == "Nist")
            {
              errorModel = CreateObject<NistErrorRateModel>();
            }
          else if (modelNames[modelIndex] == "Yans")
            {
              errorModel = CreateObject<YansErrorRateModel>();
            }
          else
            {
              errorModel = CreateObject<TableBasedErrorRateModel>();
            }

          uint32_t frameBits = 8 * frameSize;
          for (const double snrDb : snrVals)
            {
              double snrLinear = DbToRatio(snrDb);
              double ber = errorModel->GetChunkSuccessRate(ofdmRec.mode, snrLinear, frameBits, false);
              ds.Add(snrDb, ber);
            }
          datasets.push_back(ds);
        }
      allDatasets[modelIndex] = datasets;
    }

  PlotGnuplot("frame-success-rate", modelNames, allDatasets, ofdmModes, snrVals);
  std::cout << "Frame Success Rate data written to frame-success-rate_*.plt\n";
  return 0;
}