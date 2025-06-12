#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ErrorRateComparison");

class ErrorRateComparison : public Object
{
public:
  ErrorRateComparison();
  void Run(uint32_t frameSize, uint8_t mcsMin, uint8_t mcsMax, double snrMin, double snrMax, double snrStep,
           bool useNist, bool useYans, bool useTable);
private:
  void CalculateAndPlot(const std::string& modelName, Gnuplot2dDataset& dataset,
                        WifiModemStandard standard, uint32_t frameSize,
                        uint8_t mcsMin, uint8_t mcsMax, double snrMin, double snrMax, double snrStep);
};

ErrorRateComparison::ErrorRateComparison()
{
}

void
ErrorRateComparison::Run(uint32_t frameSize, uint8_t mcsMin, uint8_t mcsMax, double snrMin, double snrMax, double snrStep,
                         bool useNist, bool useYans, bool useTable)
{
  Gnuplot plot;
  plot.SetTitle("Frame Error Rate vs SNR for Different Error Models");
  plot.SetTerminal("postscript eps color");
  plot.SetLegend("SNR (dB)", "FER");
  plot.AppendExtra("set key top left");

  if (useNist)
    {
      Gnuplot2dDataset nistDataset;
      nistDataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);
      nistDataset.SetColor(1);
      nistDataset.SetTitle("Nist");
      CalculateAndPlot("Nist", nistDataset, WIFI_MOD_CLASSIC, mcsMin, mcsMax, snrMin, snrMax, snrStep);
      plot.AddDataset(nistDataset);
    }

  if (useYans)
    {
      Gnuplot2dDataset yansDataset;
      yansDataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);
      yansDataset.SetColor(2);
      yansDataset.SetTitle("Yans");
      CalculateAndPlot("Yans", yansDataset, WIFI_MOD_CLASSIC, mcsMin, mcsMax, snrMin, snrMax, snrStep);
      plot.AddDataset(yansDataset);
    }

  if (useTable)
    {
      Gnuplot2dDataset tableDataset;
      tableDataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);
      tableDataset.SetColor(3);
      tableDataset.SetTitle("Table-Based");
      CalculateAndPlot("TableBased", tableDataset, WIFI_MOD_CLASSIC, mcsMin, mcsMax, snrMin, snrMax, snrStep);
      plot.AddDataset(tableDataset);
    }

  std::ofstream plotFile("error-rate-comparison.plt");
  plot.GenerateOutput(plotFile);
  plotFile.close();

  std::ofstream epsFile("error-rate-comparison.eps");
  plot.GenerateOutput(epsFile);
  epsFile.close();
}

void
ErrorRateComparison::CalculateAndPlot(const std::string& modelName, Gnuplot2dDataset& dataset,
                                      WifiModemStandard standard, uint32_t frameSize,
                                      uint8_t mcsMin, uint8_t mcsMax, double snrMin, double snrMax, double snrStep)
{
  Config::SetDefault("ns3::WifiPhy::ErrorModelType", StringValue(modelName + "ErrorModel"));

  Ptr<WifiModem> modem = CreateObject<WifiModem>();
  modem->SetStandard(standard);

  for (double snr = snrMin; snr <= snrMax; snr += snrStep)
    {
      double totalFer = 0.0;
      uint8_t count = 0;
      for (uint8_t mcs = mcsMin; mcs <= mcsMax && mcs <= 7; ++mcs)
        {
          WifiMode mode = WifiPhy::GetOfdmRateFromMcs(mcs);
          if (!mode.IsValid())
            continue;

          double fer = modem->CalculateFer(frameSize * 8, mode, Seconds(1), snr, 0.0);
          totalFer += fer;
          ++count;
        }
      if (count > 0)
        {
          dataset.Add(snr, totalFer / count);
        }
    }
}

int main(int argc, char *argv[])
{
  uint32_t frameSize = 1500;
  uint8_t mcsMin = 0;
  uint8_t mcsMax = 7;
  double snrMin = -5;
  double snrMax = 30;
  double snrStep = 1;
  bool useNist = true;
  bool useYans = true;
  bool useTable = true;

  CommandLine cmd(__FILE__);
  cmd.AddValue("frameSize", "The size of the frame in bytes", frameSize);
  cmd.AddValue("mcsMin", "Minimum MCS index to test", mcsMin);
  cmd.AddValue("mcsMax", "Maximum MCS index to test", mcsMax);
  cmd.AddValue("snrMin", "Minimum SNR in dB", snrMin);
  cmd.AddValue("snrMax", "Maximum SNR in dB", snrMax);
  cmd.AddValue("snrStep", "SNR step in dB", snrStep);
  cmd.AddValue("useNist", "Use Nist error model", useNist);
  cmd.AddValue("useYans", "Use Yans error model", useYans);
  cmd.AddValue("useTable", "Use Table-based error model", useTable);
  cmd.Parse(argc, argv);

  ErrorRateComparison erc;
  erc.Run(frameSize, mcsMin, mcsMax, snrMin, snrMax, snrStep, useNist, useYans, useTable);

  return 0;
}