#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("EhtErrorRateValidation");

class EhtErrorRateComparison : public Object
{
public:
  static TypeId GetTypeId(void);
  EhtErrorRateComparison();
  void RunSimulation();

private:
  void GeneratePlots();
  double CalculateFsrForModel(Ptr<ErrorRateModel> model, WifiMode mode, uint32_t frameSize, double snr);

  uint32_t m_frameSize;
  double m_minSnr;
  double m_maxSnr;
  double m_snrStep;
};

TypeId
EhtErrorRateComparison::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::EhtErrorRateComparison")
                          .SetParent<Object>()
                          .AddConstructor<EhtErrorRateComparison>();
  return tid;
}

EhtErrorRateComparison::EhtErrorRateComparison()
    : m_frameSize(1500), m_minSnr(-5.0), m_maxSnr(30.0), m_snrStep(1.0)
{
}

double
EhtErrorRateComparison::CalculateFsrForModel(Ptr<ErrorRateModel> model, WifiMode mode, uint32_t size, double snr)
{
  uint32_t payloadBits = size * 8;
  double ber = model->GetChunkSuccessRate(mode, snr, payloadBits);
  double fsr = ber;
  return fsr;
}

void
EhtErrorRateComparison::RunSimulation()
{
  GnuplotCollection gnuplots("eht-error-rate-comparison.plt");
  gnuplots.SetTerminal("png");
  gnuplots.AddDataset("Frame Success Rate vs SNR Comparison");

  std::vector<std::string> models = {"Nist", "Yans", "Table"};
  std::vector<WifiMode> modes;
  for (uint8_t mcs = 0; mcs <= 13; ++mcs)
  {
    modes.push_back(WifiPhy::GetEhtMcs(mcs));
  }

  for (auto modelType : models)
  {
    Gnuplot2dDataset dataset(modelType);
    dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    Ptr<ErrorRateModel> model;
    if (modelType == "Nist")
    {
      model = CreateObject<NistErrorRateModel>();
    }
    else if (modelType == "Yans")
    {
      model = CreateObject<YansErrorRateModel>();
    }
    else if (modelType == "Table")
    {
      model = CreateObject<TableBasedErrorRateModel>();
    }

    for (double snr = m_minSnr; snr <= m_maxSnr; snr += m_snrStep)
    {
      double avgFsr = 0.0;
      for (const auto& mode : modes)
      {
        avgFsr += CalculateFsrForModel(model, mode, m_frameSize, snr);
      }
      avgFsr /= modes.size();
      dataset.Add(snr, avgFsr);
    }

    gnuplots.AddDataset(dataset);
  }

  gnuplots.GenerateOutput();
}

void
EhtErrorRateComparison::GeneratePlots()
{
  // This function can be extended to generate more detailed plots per MCS if needed
  NS_LOG_UNCOND("Plots generated using Gnuplot.");
}

int
main(int argc, char* argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  EhtErrorRateComparison comparison;
  comparison.RunSimulation();
  comparison.GeneratePlots();

  return 0;
}