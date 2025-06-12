#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HeErrorRateModelComparison");

class HeFrameSuccessRateExperiment
{
public:
  HeFrameSuccessRateExperiment();
  void Run(uint32_t frameSize);
  void SetModels(std::vector<Ptr<ErrorRateModel>> models);
  void SetMcsValues(std::vector<uint8_t> mcsValues);
  void SetSnrs(std::vector<double> snrs);

private:
  std::vector<Ptr<ErrorRateModel>> m_models;
  std::vector<uint8_t> m_mcsValues;
  std::vector<double> m_snrs;
  uint32_t m_frameSize;
};

HeFrameSuccessRateExperiment::HeFrameSuccessRateExperiment()
    : m_frameSize(1000)
{
}

void
HeFrameSuccessRateExperiment::SetModels(std::vector<Ptr<ErrorRateModel>> models)
{
  m_models = models;
}

void
HeFrameSuccessRateExperiment::SetMcsValues(std::vector<uint8_t> mcsValues)
{
  m_mcsValues = mcsValues;
}

void
HeFrameSuccessRateExperiment::SetSnrs(std::vector<double> snrs)
{
  m_snrs = snrs;
}

void
HeFrameSuccessRateExperiment::Run(uint32_t frameSize)
{
  m_frameSize = frameSize;

  WifiTxVector txVector;
  txVector.SetPreambleType(WIFI_PREAMBLE_HE_SU);
  txVector.SetChannelWidth(20);
  txVector.SetGuardInterval(800);

  GnuplotCollection gnuplots("he-error-rate-comparison");
  gnuplots.SetTitle("Frame Success Rate vs SNR for HE MCS values");
  gnuplots.SetTerminal("png");
  gnuplots.SetOutputFilename("he-error-rate-comparison");

  for (auto mcs : m_mcsValues)
    {
      txVector.SetMode(WifiUtils::GetHeMcs(mcs));

      std::ostringstream oss;
      oss << "mcs-" << static_cast<uint32_t>(mcs);
      Gnuplot2dDataset dataset(oss.str());
      Gnuplot plot(oss.str() + ".dat");
      plot.SetTitle("HE MCS" + std::to_string(mcs));
      plot.SetXtitle("SNR (dB)");
      plot.SetYtitle("Frame Success Rate");

      for (auto model : m_models)
        {
          std::string modelName = model->GetInstanceTypeId().GetName();
          if (modelName.find("Nist") != std::string::npos)
            {
              plot.SetDatasetName("NIST MCS" + std::to_string(mcs));
            }
          else if (modelName.find("Yans") != std::string::npos)
            {
              plot.SetDatasetName("YANS MCS" + std::to_string(mcs));
            }
          else if (modelName.find("Table") != std::string::npos)
            {
              plot.SetDatasetName("Table MCS" + std::to_string(mcs));
            }

          for (auto snrDb : m_snrs)
            {
              double successRate = 1.0 - model->GetChunkSuccessRate(txVector, 0, m_frameSize * 8,
                                                                   WattsToDbm(SnrToPower(snrDb)));
              plot.Add(Gnuplot2dPoint(snrDb, successRate));
            }
          dataset = plot.GetDataset();
          gnuplots.AddPlot(plot);
        }
    }

  gnuplots.GenerateOutput();
}

int
main(int argc, char* argv[])
{
  uint32_t frameSize = 1000;
  CommandLine cmd(__FILE__);
  cmd.AddValue("frameSize", "The size of the transmitted frame in bytes", frameSize);
  cmd.Parse(argc, argv);

  std::vector<Ptr<ErrorRateModel>> models;
  models.push_back(CreateObject<NistErrorRateModel>());
  models.push_back(CreateObject<YansErrorRateModel>());
  models.push_back(CreateObject<TableBasedErrorRateModel>());

  std::vector<uint8_t> mcsValues;
  for (uint8_t i = 0; i <= 11; ++i)
    {
      mcsValues.push_back(i);
    }

  std::vector<double> snrs;
  for (double snr = -10; snr <= 30; snr += 2)
    {
      snrs.push_back(snr);
    }

  HeFrameSuccessRateExperiment experiment;
  experiment.SetModels(models);
  experiment.SetMcsValues(mcsValues);
  experiment.SetSnrs(snrs);
  experiment.Run(frameSize);

  return 0;
}