#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ErrorRateComparison");

class ErrorRateComparison
{
public:
  ErrorRateComparison();
  void Run(uint32_t frameSize, uint8_t mcsStart, uint8_t mcsEnd, uint8_t mcsStep,
           bool useNist, bool useYans, bool useTable);
private:
  void SetupSimulation();
  double CalculateFer(Ptr<ErrorRateModel> errorModel, WifiMode mode, double snr, uint32_t size);

  Gnuplot2dDataset m_nistDataset;
  Gnuplot2dDataset m_yansDataset;
  Gnuplot2dDataset m_tableDataset;
};

ErrorRateComparison::ErrorRateComparison()
  : m_nistDataset("Nist"),
    m_yansDataset("Yans"),
    m_tableDataset("Table")
{
  m_nistDataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);
  m_yansDataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);
  m_tableDataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);
}

void
ErrorRateComparison::Run(uint32_t frameSize, uint8_t mcsStart, uint8_t mcsEnd, uint8_t mcsStep,
                         bool useNist, bool useYans, bool useTable)
{
  SetupSimulation();

  for (uint8_t mcs = mcsStart; mcs <= mcsEnd; mcs += mcsStep)
    {
      WifiMode mode = WifiPhy::GetHeMcs(mcs);
      if (!mode.IsDefined())
        {
          NS_LOG_WARN("Invalid MCS value: " << static_cast<uint16_t>(mcs));
          continue;
        }

      for (double snrDb = -10.0; snrDb <= 30.0; snrDb += 2.0)
        {
          double snr = std::pow(10.0, snrDb / 10.0);
          if (useNist)
            {
              double fer = CalculateFer(CreateObject<NistErrorRateModel>(), mode, snr, frameSize);
              m_nistDataset.Add(snrDb, fer);
            }
          if (useYans)
            {
              double fer = CalculateFer(CreateObject<YansErrorRateModel>(), mode, snr, frameSize);
              m_yansDataset.Add(snrDb, fer);
            }
          if (useTable)
            {
              double fer = CalculateFer(CreateObject<TableBasedErrorRateModel>(), mode, snr, frameSize);
              m_tableDataset.Add(snrDb, fer);
            }
        }
    }

  Gnuplot gnuplot("error-rate-comparison.eps");
  gnuplot.SetTerminal("postscript eps color enhanced");
  gnuplot.SetTitle("Frame Error Rate vs SNR for Different Error Models");
  gnuplot.SetLegend("SNR (dB)", "Frame Error Rate");
  gnuplot.SetExtra("set logscale y\nset grid");

  if (useNist) gnuplot.AddDataset(m_nistDataset);
  if (useYans) gnuplot.AddDataset(m_yansDataset);
  if (useTable) gnuplot.AddDataset(m_tableDataset);

  std::ofstream plotFile("error-rate-comparison.plt");
  gnuplot.GenerateOutput(plotFile);
}

void
ErrorRateComparison::SetupSimulation()
{
  Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue("OfdmRate24Mbps"));
  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("999999"));
  Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue("999999"));
}

double
ErrorRateComparison::CalculateFer(Ptr<ErrorRateModel> errorModel, WifiMode mode, double snr, uint32_t size)
{
  double ber = errorModel->GetBitErrorRate(mode, snr);
  return 1.0 - std::pow(1.0 - ber, size * 8);
}

int main(int argc, char *argv[])
{
  uint32_t frameSize = 1500;
  uint8_t mcsStart = 0;
  uint8_t mcsEnd = 7;
  uint8_t mcsStep = 1;
  bool useNist = true;
  bool useYans = true;
  bool useTable = true;

  CommandLine cmd(__FILE__);
  cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
  cmd.AddValue("mcsStart", "Starting MCS index", mcsStart);
  cmd.AddValue("mcsEnd", "Ending MCS index", mcsEnd);
  cmd.AddValue("mcsStep", "Step between MCS indices", mcsStep);
  cmd.AddValue("useNist", "Use Nist error model", useNist);
  cmd.AddValue("useYans", "Use Yans error model", useYans);
  cmd.AddValue("useTable", "Use Table-based error model", useTable);
  cmd.Parse(argc, argv);

  ErrorRateComparison simulation;
  simulation.Run(frameSize, mcsStart, mcsEnd, mcsStep, useNist, useYans, useTable);

  return 0;
}