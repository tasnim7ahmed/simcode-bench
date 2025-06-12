#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

struct ErrorRateModelConfig
{
  std::string name;
  Ptr<ErrorRateModel> (*create)();
};

static Ptr<ErrorRateModel> CreateNist ()
{
  return CreateObject<NistErrorRateModel> ();
}

static Ptr<ErrorRateModel> CreateYans ()
{
  return CreateObject<YansErrorRateModel> ();
}

static Ptr<ErrorRateModel> CreateTable ()
{
  Ptr<TableBasedErrorRateModel> model = CreateObject<TableBasedErrorRateModel> ();
  model->LoadDefaults ();
  return model;
}

std::vector<ErrorRateModelConfig> errorRateModels = {
  { "NIST",  &CreateNist },
  { "YANS",  &CreateYans },
  { "Table", &CreateTable }
};

struct ModeConfig
{
  std::string name;
  WifiMode wifiMode;
};

int main (int argc, char *argv[])
{
  uint32_t frameSize = 1200;

  CommandLine cmd;
  cmd.AddValue ("frameSize", "Frame size in bytes", frameSize);
  cmd.Parse (argc, argv);

  // Find available OFDM WifiModes (IEEE 802.11a/g 6Mbps to 54Mbps)
  std::vector<ModeConfig> ofdmModes;
  const WifiPhyHelper phy = WifiPhyHelper::Default ();
  // Cache standard rates
  WifiPhyStandard standard = WIFI_PHY_STANDARD_80211a;
  WifiHelper wifiHelper;
  wifiHelper.SetStandard (standard);

  Ptr<YansWifiPhy> dummyPhy = CreateObject<YansWifiPhy> ();
  std::vector<WifiMode> wifiModeList = wifiHelper.GetPhyModes (standard);

  for (const WifiMode &mode : wifiModeList)
    {
      std::string modClass = mode.GetModulationClassString ();
      if (modClass == "OFDM")
        {
          ofdmModes.push_back ({mode.GetUniqueName (), mode});
        }
    }

  // SNR: -5 to 30 dB
  const double snrStart = -5.0;
  const double snrEnd = 30.0;
  const double snrStep = 1.0;

  for (const auto &errModelConf : errorRateModels)
    {
      Gnuplot plot ("fsr-vs-snr-" + errModelConf.name + ".png");
      plot.SetTitle ("Frame Success Rate vs SNR (" + errModelConf.name + ")");
      plot.SetLegend ("SNR (dB)", "Frame Success Rate");

      for (const auto &modeConf : ofdmModes)
        {
          Gnuplot2dDataset dataset;
          dataset.SetTitle (modeConf.name);
          dataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

          Ptr<ErrorRateModel> errModel = (*errModelConf.create) ();

          for (double snrDb = snrStart; snrDb <= snrEnd + 1e-6; snrDb += snrStep)
            {
              double snrLinear = std::pow (10.0, snrDb/10.0);
              double per = errModel->GetChunkSuccessRate (modeConf.wifiMode, snrLinear, frameSize * 8);
              dataset.Add (snrDb, per);
            }
          plot.AddDataset (dataset);
        }

      std::ofstream plotFile ("fsr-vs-snr-" + errModelConf.name + ".plt");
      plot.GenerateOutput (plotFile);
      plotFile.close ();
    }

  return 0;
}