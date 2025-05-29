#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <string>
#include <map>

using namespace ns3;

static std::map<std::string, WifiMode> GetOfdmModes ()
{
  std::map<std::string, WifiMode> ofdmModes;
  ofdmModes["OfdmRate6Mbps"] = WifiPhyHelper::Default ()->GetStandard ().IsOfdm () ? WifiMode ("OfdmRate6Mbps") : WifiMode ("");
  ofdmModes["OfdmRate9Mbps"] = WifiMode ("OfdmRate9Mbps");
  ofdmModes["OfdmRate12Mbps"] = WifiMode ("OfdmRate12Mbps");
  ofdmModes["OfdmRate18Mbps"] = WifiMode ("OfdmRate18Mbps");
  ofdmModes["OfdmRate24Mbps"] = WifiMode ("OfdmRate24Mbps");
  ofdmModes["OfdmRate36Mbps"] = WifiMode ("OfdmRate36Mbps");
  ofdmModes["OfdmRate48Mbps"] = WifiMode ("OfdmRate48Mbps");
  ofdmModes["OfdmRate54Mbps"] = WifiMode ("OfdmRate54Mbps");
  for (auto it = ofdmModes.begin (); it != ofdmModes.end (); )
    {
      if (!it->second.IsValid ())
        it = ofdmModes.erase (it);
      else
        ++it;
    }
  return ofdmModes;
}

struct ModelInfo
{
  std::string modelName;
  Ptr<ErrorRateModel> (*Create)();
};

Ptr<ErrorRateModel> CreateNistModel () { return CreateObject<NistErrorRateModel> (); }
Ptr<ErrorRateModel> CreateYansModel () { return CreateObject<YansErrorRateModel> (); }
Ptr<ErrorRateModel> CreateTableModel () { return CreateObject<TableBasedErrorRateModel> (); }

static void
CalculateFrResult (Ptr<ErrorRateModel> errorRateModel,
                   WifiMode mode,
                   int frameBytes,
                   double snrDb,
                   double &frameSuccessRate)
{
  double snrLinear = std::pow (10.0, snrDb / 10.0);
  const uint8_t * dummy = nullptr;
  double per = errorRateModel->GetChunkSuccessRate (mode, dummy, frameBytes * 8, snrLinear);
  frameSuccessRate = per;
}

int 
main (int argc, char *argv[])
{
  uint32_t frameBytes = 1500;
  CommandLine cmd;
  cmd.AddValue ("frameBytes", "Frame size in bytes", frameBytes);
  cmd.Parse (argc, argv);

  std::map<std::string, WifiMode> ofdmModes = GetOfdmModes ();
  std::vector<ModelInfo> models = {
      { "NIST", &CreateNistModel },
      { "YANS", &CreateYansModel },
      { "TABLE", &CreateTableModel },
  };

  for (const auto& modelInfo : models)
    {
      std::string plotTitle = "Frame Success Rate vs SNR [" + modelInfo.modelName + " Error Model]";
      std::string fileName = "fsr_snr_ofdm_" + modelInfo.modelName + ".plt";
      Gnuplot plot (fileName);
      plot.SetTitle (plotTitle);
      plot.SetTerminal ("png");
      plot.SetLegend ("SNR [dB]", "Frame Success Rate");
      plot.SetExtra ("set xrange [-5:30]\nset yrange [0:1]\nset grid");

      for (const auto& modeEntry : ofdmModes)
        {
          std::string modeName = modeEntry.first;
          WifiMode mode = modeEntry.second;
          Gnuplot2dDataset dataset;
          dataset.SetTitle (modeName);
          dataset.SetStyle (Gnuplot2dDataset::LINES);

          for (int snrDb = -5; snrDb <= 30; snrDb++)
            {
              Ptr<ErrorRateModel> errorRateModel = modelInfo.Create ();
              double fsr = 0.0;
              CalculateFrResult (errorRateModel, mode, frameBytes, (double)snrDb, fsr);
              dataset.Add ((double)snrDb, fsr);
            }
          plot.AddDataset (dataset);
        }
      std::ofstream plotFile (fileName.c_str ());
      plot.GenerateOutput (plotFile);
      plotFile.close ();
    }

  return 0;
}