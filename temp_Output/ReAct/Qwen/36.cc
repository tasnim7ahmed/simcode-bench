#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/propagation-module.h"
#include "ns3/mobility-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VhtErrorRateModelComparison");

class VhtErrorRateExperiment
{
public:
  VhtErrorRateExperiment();
  void Run(uint32_t frameSize);
private:
  void CalculateFrameSuccessRate(double snrDb, WifiMode mode, WifiPreamble preamble, const std::string& errorModel, Gnuplot2dDataset& dataset);
  void PlotResults(const std::map<std::string, Gnuplot>& plots) const;
};

VhtErrorRateExperiment::VhtErrorRateExperiment()
{
}

void
VhtErrorRateExperiment::Run(uint32_t frameSize)
{
  std::vector<std::string> errorModels = {"Nist", "Yans", "Table"};
  std::vector<WifiMode> modes;

  for (uint8_t mcs = 0; mcs <= 9; mcs++)
    {
      WifiMode mode = WifiPhy::GetVhtMcs(mcs);
      if (mcs == 9)
        {
          // Skip MCS 9 for 20 MHz
          continue;
        }
      modes.push_back(mode);
    }

  std::map<std::string, Gnuplot> plots;

  for (const auto& errorModel : errorModels)
    {
      Gnuplot plot;
      plot.SetTitle("Frame Success Rate vs SNR (" + errorModel + ")");
      plot.SetXTitle("SNR (dB)");
      plot.SetYTitle("Frame Success Rate");
      plots[errorModel] = plot;
    }

  for (double snrDb = -5; snrDb <= 30; snrDb += 1)
    {
      for (const auto& errorModel : errorModels)
        {
          Config::SetDefault("ns3::WifiPhy::ErrorRateModel", StringValue("ns3::" + errorModel + "ErrorRateModel"));
          for (auto mode : modes)
            {
              Gnuplot2dDataset dataset;
              dataset.SetTitle("MCS " + std::to_string(mode.GetMcsValue()));
              CalculateFrameSuccessRate(snrDb, mode, WIFI_PREAMBLE_VHT, errorModel, dataset);
              plots[errorModel].AddDataset(dataset);
            }
        }
    }

  PlotResults(plots);
}

void
VhtErrorRateExperiment::CalculateFrameSuccessRate(double snrDb, WifiMode mode, WifiPreamble preamble,
                                                  const std::string& errorModel, Gnuplot2dDataset& dataset)
{
  Ptr<WifiPhy> phy = CreateObject<WifiPhy>();
  phy->SetChannelWidth(20); // MHz
  phy->SetFrequency(5180);  // MHz
  phy->SetErrorRateModel(errorModel);

  double ber = phy->GetPhyEntity(WIFI_MOD_CLASS_VHT)->GetBinaryCodeWordErrorRate(snrDb, mode, preamble);
  double ps = pow((1.0 - ber), mode.GetMcsValue() * 4.0 * 1000 / 8.0); // crude approximation

  dataset.Add(snrDb, ps);
}

void
VhtErrorRateExperiment::PlotResults(const std::map<std::string, Gnuplot>& plots) const
{
  for (const auto& pair : plots)
    {
      std::ostringstream oss;
      oss << "vht-error-rate-" << pair.first << ".plt";
      std::ofstream pltFile(oss.str().c_str());
      pair.second.GenerateOutput(pltFile);
    }
}

int
main(int argc, char* argv[])
{
  uint32_t frameSize = 1000;
  CommandLine cmd(__FILE__);
  cmd.AddValue("frameSize", "The frame size in bytes", frameSize);
  cmd.Parse(argc, argv);

  VhtErrorRateExperiment experiment;
  experiment.Run(frameSize);

  return 0;
}