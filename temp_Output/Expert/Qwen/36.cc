#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include <fstream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VhtErrorRatePlot");

class VhtErrorRateComparison
{
public:
  VhtErrorRateComparison();
  void Run(uint32_t frameSize);
  void PlotResults();

private:
  void CalculateFrameSuccessRate(double snrDb, WifiMode mode, WifiTxVector txVector,
                                 const Ptr<ErrorRateModel> errorModel, double* successRate);

  std::map<std::string, std::vector<std::pair<double, double>>> m_results;
};

VhtErrorRateComparison::VhtErrorRateComparison()
{
  m_results["Nist"] = {};
  m_results["Yans"] = {};
  m_results["Table"] = {};
}

void
VhtErrorRateComparison::CalculateFrameSuccessRate(double snrDb, WifiMode mode,
                                                  WifiTxVector txVector,
                                                  const Ptr<ErrorRateModel> errorModel,
                                                  double* successRate)
{
  double ber = errorModel->GetChunkSuccessRate(mode, txVector, snrDb, txVector.GetMode().GetDataRate(txVector));
  *successRate = pow(1 - ber, (8 * txVector.GetSize()));
}

void
VhtErrorRateComparison::Run(uint32_t frameSize)
{
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211_AC);

  auto nistModel = CreateObject<NistErrorRateModel>();
  auto yansModel = CreateObject<YansErrorRateModel>();
  auto tableModel = CreateObject<TableBasedErrorRateModel>();

  uint16_t channelWidths[] = {20, 40, 80, 160};
  for (auto width : channelWidths)
    {
      for (uint8_t mcs = 0; mcs <= 9; ++mcs)
        {
          if (width == 20 && mcs == 9)
            continue; // Skip MCS 9 for 20 MHz

          WifiMode mode = WifiPhy::GetVhtMcs(mcs);
          WifiTxVector txVector(mode, 0, WIFI_PREAMBLE_VHT_SU, 800, 1, 1, 0, width, false);

          for (double snrDb = -5; snrDb <= 30; snrDb += 1.0)
            {
              double successRate;

              CalculateFrameSuccessRate(snrDb, mode, txVector, nistModel, &successRate);
              m_results["Nist"].push_back(std::make_pair(snrDb, successRate));

              CalculateFrameSuccessRate(snrDb, mode, txVector, yansModel, &successRate);
              m_results["Yans"].push_back(std::make_pair(snrDb, successRate));

              CalculateFrameSuccessRate(snrDb, mode, txVector, tableModel, &successRate);
              m_results["Table"].push_back(std::make_pair(snrDb, successRate));
            }

          std::ostringstream oss;
          oss << "VHT-MCS" << static_cast<uint16_t>(mcs) << "-" << width << "MHz";
          PlotResults();
          m_results["Nist"].clear();
          m_results["Yans"].clear();
          m_results["Table"].clear();
        }
    }
}

void
VhtErrorRateComparison::PlotResults()
{
  Gnuplot plot;
  plot.SetTitle("Frame Success Rate vs SNR");
  plot.SetXTitle("SNR (dB)");
  plot.SetYTitle("Frame Success Rate");

  Gnuplot2dDataset datasetNist("NIST Model");
  datasetNist.SetStyle(Gnuplot2dDataset::LINES_POINTS);
  for (const auto& p : m_results["Nist"])
    {
      datasetNist.Add(p.first, p.second);
    }

  Gnuplot2dDataset datasetYans("YANS Model");
  datasetYans.SetStyle(Gnuplot2dDataset::LINES_POINTS);
  for (const auto& p : m_results["Yans"])
    {
      datasetYans.Add(p.first, p.second);
    }

  Gnuplot2dDataset datasetTable("Table Model");
  datasetTable.SetStyle(Gnuplot2dDataset::LINES_POINTS);
  for (const auto& p : m_results["Table"])
    {
      datasetTable.Add(p.first, p.second);
    }

  plot.AddDataset(datasetNist);
  plot.AddDataset(datasetYans);
  plot.AddDataset(datasetTable);

  std::ostringstream filename;
  filename << "vht_error_rate_comparison-" << Simulator::Now().GetSeconds() << ".plt";
  std::ofstream plotFile(filename.str());
  plot.GenerateOutput(plotFile);
  plotFile.close();
}

int
main(int argc, char* argv[])
{
  uint32_t frameSize = 1472;
  CommandLine cmd(__FILE__);
  cmd.AddValue("frameSize", "The size of the frame in bytes", frameSize);
  cmd.Parse(argc, argv);

  VhtErrorRateComparison simulation;
  simulation.Run(frameSize);

  Simulator::Destroy();
  return 0;
}