#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <sstream>
#include <iomanip>

using namespace ns3;

struct ErrorModelEntry
{
  std::string name;
  Ptr<WifiPhy> phy;
  Ptr<ErrorRateModel> model;
  Gnuplot2dDataset dataset_mcs0;
  Gnuplot2dDataset dataset_mcs4;
  Gnuplot2dDataset dataset_mcs7;
  std::string color;
};

static uint32_t g_framesize = 1200;
static double g_snrMin = 0.0;
static double g_snrMax = 30.0;
static double g_snrStep = 1.0;
static bool g_useNist = true;
static bool g_useYans = true;
static bool g_useTable = true;

static std::string g_output = "fer_vs_snr.eps";

static void
AddCommandLineParameters()
{
  CommandLine cmd;
  cmd.AddValue("framesize", "Frame size in bytes", g_framesize);
  cmd.AddValue("snrMin", "Minimum SNR in dB", g_snrMin);
  cmd.AddValue("snrMax", "Maximum SNR in dB", g_snrMax);
  cmd.AddValue("snrStep", "SNR step in dB", g_snrStep);
  cmd.AddValue("useNist", "Enable NistErrorRateModel", g_useNist);
  cmd.AddValue("useYans", "Enable YansErrorRateModel", g_useYans);
  cmd.AddValue("useTable", "Enable TableBasedErrorRateModel", g_useTable);
  cmd.AddValue("output", "Output EPS plot file", g_output);
  cmd.Parse (g_cmd_args);
}

static double
CalcFer(Ptr<ErrorRateModel> errModel, WifiPhyStandard std, WifiMode mode, double snrDb, uint32_t frameSize)
{
  double snrLinear = std::pow (10.0, snrDb / 10.0);
  WifiTxVector txVector;
  txVector.SetMode(mode);
  txVector.SetNss(1);
  txVector.SetPreambleType(WIFI_PREAMBLE_LONG);

  double packetErrorRate = errModel->GetChunkSuccessRate (txVector, frameSize * 8, snrLinear, WIFI_PREAMBLE_LONG);
  return 1.0 - packetErrorRate;
}

static void
ConfigureModels(std::vector<ErrorModelEntry> &entries, WifiPhyStandard standard)
{
  if (g_useNist)
    {
      Ptr<NistErrorRateModel> nist = CreateObject<NistErrorRateModel> ();
      ErrorModelEntry entry;
      entry.name = "Nist";
      entry.model = nist;
      entry.color = "red";
      entries.push_back(entry);
    }
  if (g_useYans)
    {
      Ptr<YansErrorRateModel> yans = CreateObject<YansErrorRateModel> ();
      ErrorModelEntry entry;
      entry.name = "Yans";
      entry.model = yans;
      entry.color = "blue";
      entries.push_back(entry);
    }
  if (g_useTable)
    {
      Ptr<TableBasedErrorRateModel> tbl = CreateObject<TableBasedErrorRateModel> ();
      ErrorModelEntry entry;
      entry.name = "TableBased";
      entry.model = tbl;
      entry.color = "green";
      entries.push_back(entry);
    }
}

static WifiMode GetMcsMode(uint8_t mcs)
{
  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n_2_4GHZ);
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  Ptr<WifiPhy> phy = wifiPhy.Create();
  std::vector<WifiMode> modes = phy->GetModeList();
  for (const auto &mode : modes)
    {
      if (mode.GetModulationClass() == WIFI_MOD_CLASS_HT)
        {
          if (mode.GetMcsValue() == mcs)
            {
              return mode;
            }
        }
    }
  NS_ABORT_MSG_UNLESS(false, "No suitable WifiMode found for MCS " << unsigned(mcs));
}

int
main(int argc, char *argv[])
{
  AddCommandLineParameters();

  std::vector<ErrorModelEntry> errorModels;
  ConfigureModels(errorModels, WIFI_PHY_STANDARD_80211n_2_4GHZ);

  std::vector<uint8_t> mcsToPlot = {0, 4, 7};
  std::vector<std::string> mcsLabels = {"MCS 0", "MCS 4", "MCS 7"};

  for (auto& modelEntry : errorModels)
    {
      modelEntry.dataset_mcs0.SetTitle(modelEntry.name + " " + mcsLabels[0]);
      modelEntry.dataset_mcs0.SetStyle(Gnuplot2dDataset::LINES);
      modelEntry.dataset_mcs0.SetExtra("lw 2 lc rgb '" + modelEntry.color + "'");

      modelEntry.dataset_mcs4.SetTitle(modelEntry.name + " " + mcsLabels[1]);
      modelEntry.dataset_mcs4.SetStyle(Gnuplot2dDataset::LINES);
      modelEntry.dataset_mcs4.SetExtra("dt 2 lw 2 lc rgb '" + modelEntry.color + "'");

      modelEntry.dataset_mcs7.SetTitle(modelEntry.name + " " + mcsLabels[2]);
      modelEntry.dataset_mcs7.SetStyle(Gnuplot2dDataset::LINES);
      modelEntry.dataset_mcs7.SetExtra("dt 4 lw 2 lc rgb '" + modelEntry.color + "'");
    }

  for (size_t midx = 0; midx < mcsToPlot.size(); ++midx)
    {
      uint8_t mcs = mcsToPlot[midx];
      WifiMode mode = GetMcsMode(mcs);

      for (double snr = g_snrMin; snr <= g_snrMax + 1e-6; snr += g_snrStep)
        {
          for (auto &modelEntry : errorModels)
            {
              double fer = CalcFer(modelEntry.model, WIFI_PHY_STANDARD_80211n_2_4GHZ, mode, snr, g_framesize);

              if (mcs == 0)
                modelEntry.dataset_mcs0.Add(snr, fer);
              if (mcs == 4)
                modelEntry.dataset_mcs4.Add(snr, fer);
              if (mcs == 7)
                modelEntry.dataset_mcs7.Add(snr, fer);
            }
        }
    }

  Gnuplot plot;
  plot.SetTitle("Frame Error Rate vs SNR for different Error Rate Models");
  plot.SetTerminal("postscript eps enhanced color font 'Times,20'");
  plot.SetLegend("SNR [dB]", "Frame Error Rate (FER)");
  plot.AppendExtra("set key outside");
  plot.AppendExtra("set logscale y");
  plot.AppendExtra("set yrange [1e-4:1]");
  plot.AppendExtra("set grid ytics xtics");

  for (const auto &modelEntry : errorModels)
    {
      plot.AddDataset(modelEntry.dataset_mcs0);
      plot.AddDataset(modelEntry.dataset_mcs4);
      plot.AddDataset(modelEntry.dataset_mcs7);
    }

  std::ofstream plotFile(g_output);
  plot.GenerateOutput(plotFile);
  plotFile.close();

  return 0;
}