#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ErrorRateComparison");

class ErrorRateComparison {
public:
  ErrorRateComparison();
  void Run(uint32_t frameSize, uint32_t mcsMin, uint32_t mcsMax, double snrMin, double snrMax, double snrStep,
           bool useNist, bool useYans, bool useTable);
private:
  Gnuplot2dDataset GetFerForModel(const std::string& modelName, uint32_t frameSize,
                                  uint32_t mcsMin, uint32_t mcsMax, double snrMin, double snrMax, double snrStep);

  void ConfigureErrorModel(const std::string& model);
};

ErrorRateComparison::ErrorRateComparison() {}

void
ErrorRateComparison::ConfigureErrorModel(const std::string& model)
{
  Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue("2200"));
  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));
  if (model == "Nist")
    {
      Config::SetDefault("ns3::WifiPhy::ErrorRateModel", TypeIdValue(NistErrorRateModel::GetTypeId()));
    }
  else if (model == "Yans")
    {
      Config::SetDefault("ns3::WifiPhy::ErrorRateModel", TypeIdValue(YansErrorRateModel::GetTypeId()));
    }
  else if (model == "Table")
    {
      Config::SetDefault("ns3::WifiPhy::ErrorRateModel", TypeIdValue(WifiTableErrorRateModel::GetTypeId()));
    }
}

Gnuplot2dDataset
ErrorRateComparison::GetFerForModel(const std::string& modelName, uint32_t frameSize,
                                    uint32_t mcsMin, uint32_t mcsMax, double snrMin, double snrMax, double snrStep)
{
  ConfigureErrorModel(modelName);

  Gnuplot2dDataset dataset;
  dataset.SetTitle(modelName);
  if (modelName == "Nist") dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);
  else if (modelName == "Yans") dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);
  else if (modelName == "Table") dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("HtMcs" + std::to_string(mcsMin)),
                               "ControlMode", StringValue("HtMcs" + std::to_string(mcsMin)));

  NodeContainer nodes;
  nodes.Create(2);

  NetDeviceContainer devices = wifi.Install(nodes);

  Ptr<WifiNetDevice> device = DynamicCast<WifiNetDevice>(devices.Get(0));
  Ptr<WifiPhy> phy = device->GetPhy();

  for (double snr = snrMin; snr <= snrMax; snr += snrStep)
    {
      double fer = 0.0;
      for (uint32_t mcs = mcsMin; mcs <= mcsMax; mcs++)
        {
          WifiTxVector txVector;
          txVector.SetMode(WifiModeFactory::CreateWifiMode("HtMcs" + std::to_string(mcs)));
          txVector.SetNss(1);

          double thisFer = phy->CalculateFer(frameSize * 8, txVector, SnrToRatio(snr));
          fer += thisFer;
        }
      fer /= (mcsMax - mcsMin + 1);
      dataset.Add(snr, fer);
    }

  Simulator::Destroy();
  return dataset;
}

void
ErrorRateComparison::Run(uint32_t frameSize, uint32_t mcsMin, uint32_t mcsMax, double snrMin, double snrMax, double snrStep,
                         bool useNist, bool useYans, bool useTable)
{
  Gnuplot plot;
  plot.SetTitle("Frame Error Rate vs SNR");
  plot.SetTerminal("postscript eps color enhanced");
  plot.SetLegend("SNR (dB)", "FER");
  plot.AppendExtra("set key top right");

  if (useNist)
    {
      Gnuplot2dDataset nistDataset = GetFerForModel("Nist", frameSize, mcsMin, mcsMax, snrMin, snrMax, snrStep);
      nistDataset.SetColor(1);
      plot.AddDataset(nistDataset);
    }

  if (useYans)
    {
      Gnuplot2dDataset yansDataset = GetFerForModel("Yans", frameSize, mcsMin, mcsMax, snrMin, snrMax, snrStep);
      yansDataset.SetColor(2);
      plot.AddDataset(yansDataset);
    }

  if (useTable)
    {
      Gnuplot2dDataset tableDataset = GetFerForModel("Table", frameSize, mcsMin, mcsMax, snrMin, snrMax, snrStep);
      tableDataset.SetColor(3);
      plot.AddDataset(tableDataset);
    }

  std::ofstream plotFile("error-rate-comparison.plt");
  plot.GenerateOutput(plotFile);
  plotFile.close();

  std::ofstream outFile("error-rate-comparison.eps");
  plot.GenerateOutput(outFile);
  outFile.close();
}

int
main(int argc, char* argv[])
{
  uint32_t frameSize = 1472;
  uint32_t mcsMin = 0;
  uint32_t mcsMax = 7;
  double snrMin = 0.0;
  double snrMax = 30.0;
  double snrStep = 2.0;
  bool useNist = true;
  bool useYans = true;
  bool useTable = true;

  CommandLine cmd(__FILE__);
  cmd.AddValue("frameSize", "Size of the frame in bytes", frameSize);
  cmd.AddValue("mcsMin", "Minimum MCS value to simulate", mcsMin);
  cmd.AddValue("mcsMax", "Maximum MCS value to simulate", mcsMax);
  cmd.AddValue("snrMin", "Minimum SNR in dB", snrMin);
  cmd.AddValue("snrMax", "Maximum SNR in dB", snrMax);
  cmd.AddValue("snrStep", "SNR step size in dB", snrStep);
  cmd.AddValue("useNist", "Use NIST error rate model", useNist);
  cmd.AddValue("useYans", "Use YANS error rate model", useYans);
  cmd.AddValue("useTable", "Use Table-based error rate model", useTable);
  cmd.Parse(argc, argv);

  ErrorRateComparison simulation;
  simulation.Run(frameSize, mcsMin, mcsMax, snrMin, snrMax, snrStep, useNist, useYans, useTable);

  return 0;
}