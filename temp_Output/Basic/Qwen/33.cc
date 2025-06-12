#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HtErrorRateValidation");

class HtErrorRateValidation
{
public:
  HtErrorRateValidation(uint32_t frameSize);
  void Run();
private:
  void PlotFrameSuccessRate(const std::string& modelName, WifiModulationClass modulationClass, uint8_t mcsValue, double snr, double fss);
  uint32_t m_frameSize;
};

HtErrorRateValidation::HtErrorRateValidation(uint32_t frameSize)
  : m_frameSize(frameSize)
{
}

void
HtErrorRateValidation::PlotFrameSuccessRate(const std::string& modelName, WifiModulationClass modulationClass, uint8_t mcsValue, double snr, double fss)
{
  std::ostringstream oss;
  oss << "ht-" << modelName << "-mcs" << static_cast<uint32_t>(mcsValue) << ".dat";
  Gnuplot gnuplot(oss.str());
  gnuplot.SetTitle("Frame Success Rate vs SNR for HT MCS" + std::to_string(mcsValue) + " (" + modelName + ")");
  gnuplot.SetXTitle("SNR (dB)");
  gnuplot.SetYTitle("Frame Success Rate");

  Gnuplot2dDataset dataset;
  dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

  WifiTxVector txVector;
  txVector.SetMode(WifiUtils::GetHtMcs(mcsValue));
  txVector.SetNss(1);
  txVector.SetChannelWidth(20);
  txVector.SetGuardInterval(800);

  double totalPs = 0.0;
  uint32_t nTrials = 1000;

  for (double snrDb = snr - 5; snrDb <= snr + 5; snrDb += 1)
    {
      double successCount = 0;
      for (uint32_t i = 0; i < nTrials; ++i)
        {
          double ps = 1.0;
          if (modulationClass == WIFI_MOD_CLASS_HT)
            {
              ps = WifiPhy::CalculateHtPe(txVector.GetMode(), txVector.GetNss(), snrDb, m_frameSize, txVector.GetChannelWidth(), txVector.IsShortGuardInterval(), modulationClass, true);
            }
          else if (modulationClass == WIFI_MOD_CLASS_VHT || modulationClass == WIFI_MOD_CLASS_HE)
            {
              // Not applicable here but included to cover all enum values
              ps = 1.0;
            }

          double frameSuccess = (ps < 1e-6) ? 1.0 : ((1.0 - pow(ps, m_frameSize * 8)) > 0.999999 ? 1.0 : (1.0 - pow(ps, m_frameSize * 8)));
          successCount += frameSuccess;
        }
      double fsr = successCount / nTrials;
      dataset.Add(snrDb, fsr);
    }

  gnuplot.AddDataset(dataset);
  std::ofstream plotFile(oss.str().c_str());
  gnuplot.GenerateOutput(plotFile);
  plotFile.close();
}

void
HtErrorRateValidation::Run()
{
  std::vector<std::string> models = {"Nist", "Yans", "Table"};
  std::map<std::string, Ptr<ErrorRateModel>> ermMap;

  ermMap["Nist"] = CreateObject<NistErrorRateModel>();
  ermMap["Yans"] = CreateObject<YansErrorRateModel>();
  ermMap["Table"] = CreateObject<TableBasedErrorRateModel>();

  for (const auto& modelEntry : ermMap)
    {
      std::string modelName = modelEntry.first;
      Config::SetDefault("ns3::WifiPhy::ErrorRateModel", PointerValue(modelEntry.second));

      for (uint8_t mcsIndex = 0; mcsIndex <= 31; ++mcsIndex)
        {
          NS_LOG_DEBUG("Processing " << modelName << " for HT MCS " << static_cast<uint32_t>(mcsIndex));
          PlotFrameSuccessRate(modelName, WIFI_MOD_CLASS_HT, mcsIndex, 20.0, m_frameSize);
        }
    }
}

int
main(int argc, char* argv[])
{
  uint32_t frameSize = 1472;
  CommandLine cmd(__FILE__);
  cmd.AddValue("frameSize", "The size of the frame in bytes", frameSize);
  cmd.Parse(argc, argv);

  HtErrorRateValidation simulation(frameSize);
  simulation.Run();

  return 0;
}