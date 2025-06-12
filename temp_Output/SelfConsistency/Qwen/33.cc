#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config-store-module.h"
#include "ns3/spectrum-value-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HtErrorRateModelComparison");

class HtErrorRateExperiment
{
public:
  HtErrorRateExperiment(uint32_t frameSize, std::string outputFilename);
  void Run(void);
  double GetFrameSuccessRate(double snrDb, WifiMode mode, Ptr<ErrorRateModel> errorRateModel);

private:
  uint32_t m_frameSize;
  std::string m_outputFilename;
};

HtErrorRateExperiment::HtErrorRateExperiment(uint32_t frameSize, std::string outputFilename)
    : m_frameSize(frameSize), m_outputFilename(outputFilename)
{
}

double
HtErrorRateExperiment::GetFrameSuccessRate(double snrDb, WifiMode mode, Ptr<ErrorRateModel> errorRateModel)
{
  double bitErrorRate = errorRateModel->GetBitErrorRate(snrDb, mode);
  // Frame success rate is (1 - packet error rate) where PER = 1 - (1 - BER)^frameSize*8
  return std::pow(1.0 - bitErrorRate, static_cast<double>(m_frameSize * 8));
}

void
HtErrorRateExperiment::Run()
{
  // Define the models
  std::map<std::string, Ptr<ErrorRateModel>> errorRateModels;
  errorRateModels["Nist"] = CreateObject<NistErrorRateModel>();
  errorRateModels["Yans"] = CreateObject<YansErrorRateModel>();
  errorRateModels["Table"] = CreateObject<TableBasedErrorRateModel>();

  // HT MCS values: MCS 0 to MCS 31
  std::vector<WifiMode> htModes;
  for (uint8_t mcs = 0; mcs <= 31; ++mcs)
    {
      htModes.push_back(WifiPhy::GetHtMcs(mcs));
    }

  // SNR range from 0 dB to 30 dB in steps of 2 dB
  std::vector<double> snrDbValues;
  for (double snr = 0.0; snr <= 30.0; snr += 2.0)
    {
      snrDbValues.push_back(snr);
    }

  GnuplotCollection gnuplots(m_outputFilename + ".plt");
  gnuplots.SetTitle("HT Frame Success Rate vs SNR for Different Error Rate Models");
  gnuplots.SetTerminal("png");
  gnuplots.setOutputPrefix(m_outputFilename);

  for (auto& modelPair : errorRateModels)
    {
      std::string modelName = modelPair.first;
      std::ostringstream filename;
      filename << m_outputFilename << "-" << modelName << "-fs-" << m_frameSize << ".dat";
      std::ofstream outFile(filename.str().c_str(), std::ios::out);

      Gnuplot plot(filename.str());
      plot.SetTitle(modelName + " Model - Frame Size: " + std::to_string(m_frameSize));
      plot.SetXTitle("SNR (dB)");
      plot.SetYTitle("Frame Success Rate");
      plot.SetExtra("set grid");

      for (const auto& mode : htModes)
        {
          std::string modeName = mode.GetUniqueName();
          Gnuplot2dDataset dataset;
          dataset.SetTitle(modeName);
          dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

          for (const auto& snrDb : snrDbValues)
            {
              double fsr = GetFrameSuccessRate(snrDb, mode, modelPair.second);
              dataset.Add(snrDb, fsr);
            }

          plot.AddDataset(dataset);
        }

      gnuplots.AddPlot(plot);
    }

  gnuplots.GenerateOutput();
}

int
main(int argc, char* argv[])
{
  uint32_t frameSize = 1472; // Default frame size
  std::string outputFilename = "ht-error-rate-comparison";

  CommandLine cmd(__FILE__);
  cmd.AddValue("frameSize", "The size of the frame to simulate in bytes.", frameSize);
  cmd.AddValue("output", "Base name for output files.", outputFilename);
  cmd.Parse(argc, argv);

  HtErrorRateExperiment experiment(frameSize, outputFilename);
  experiment.Run();

  return 0;
}