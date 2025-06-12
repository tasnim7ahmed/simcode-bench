#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HeErrorRateModelComparison");

class HeErrorRateComparison : public Object
{
public:
  static TypeId GetTypeId(void);
  HeErrorRateComparison();
  virtual ~HeErrorRateComparison();

  void Run(uint32_t frameSize, double snrStart, double snrEnd, uint32_t snrStep);

private:
  void GeneratePlot(std::map<std::string, std::vector<std::pair<double, double>>> results, const std::string& plotFileName);

  std::vector<uint8_t> m_mcsValues;
};

TypeId
HeErrorRateComparison::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::HeErrorRateComparison")
                          .SetParent<Object>()
                          .SetGroupName("Wifi");
  return tid;
}

HeErrorRateComparison::HeErrorRateComparison()
{
  // HE MCS values from 0 to 11 as per IEEE 802.11ax
  for (uint8_t mcs = 0; mcs <= 11; ++mcs)
    {
      m_mcsValues.push_back(mcs);
    }
}

HeErrorRateComparison::~HeErrorRateComparison()
{
}

void
HeErrorRateComparison::Run(uint32_t frameSize, double snrStart, double snrEnd, uint32_t snrStep)
{
  NS_LOG_INFO("Starting simulation with frame size: " << frameSize);

  std::map<std::string, std::vector<std::pair<double, double>>> results;

  std::vector<std::string> models = {"Nist", "Yans", "Table"};

  for (const auto& model : models)
    {
      Config::SetDefault("ns3::WifiPhy::ErrorRateModel", StringValue("ns3::" + model + "WifiManager"));
      Ptr<WifiPhy> phy = CreateObject<WifiPhy>();
      phy->ConfigureStandard(WIFI_STANDARD_80211ax_2_4GHZ);

      for (double snr = snrStart; snr <= snrEnd; snr += snrStep)
        {
          for (auto mcs : m_mcsValues)
            {
              WifiTxVector txVector;
              txVector.SetMode(WifiModeFactory::CreateWifiMode("HeMcs" + std::to_string(mcs)));
              txVector.SetChannelWidth(20);
              txVector.SetGuardInterval(800);

              double successRate = phy->CalculateFrameSuccessRate(frameSize * 8, Seconds(1), txVector, InterferenceHelper::SnrToSignalNoiseRatio(snr, phy->GetChannelWidth()));
              results[model].push_back(std::make_pair(snr, successRate));
            }
        }
    }

  GeneratePlot(results, "he-error-rate-comparison.plot");
}

void
HeErrorRateComparison::GeneratePlot(std::map<std::string, std::vector<std::pair<double, double>>> results, const std::string& plotFileName)
{
  Gnuplot plot(plotFileName);
  plot.SetTitle("Frame Success Rate vs SNR for HE MCS using different Error Rate Models");
  plot.SetTerminal("png");
  plot.SetLegend("SNR (dB)", "Frame Success Rate");
  plot.AppendExtra("set key outside right top");

  for (const auto& modelEntry : results)
    {
      Gnuplot2dDataset dataset(modelEntry.first.c_str());
      dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

      for (const auto& point : modelEntry.second)
        {
          dataset.Add(point.first, point.second);
        }

      plot.AddDataset(dataset);
    }

  std::ofstream plotFile(plotFileName.c_str());
  plot.GenerateOutput(plotFile);
  plotFile.close();
}

int
main(int argc, char* argv[])
{
  uint32_t frameSize = 1500;
  double snrStart = 0.0;
  double snrEnd = 30.0;
  uint32_t snrStep = 2;

  CommandLine cmd(__FILE__);
  cmd.AddValue("frameSize", "The size of the frame in bytes", frameSize);
  cmd.AddValue("snrStart", "Start value for SNR sweep (dB)", snrStart);
  cmd.AddValue("snrEnd", "End value for SNR sweep (dB)", snrEnd);
  cmd.AddValue("snrStep", "Step value for SNR sweep (dB)", snrStep);
  cmd.Parse(argc, argv);

  HeErrorRateComparison sim;
  sim.Run(frameSize, snrStart, snrEnd, snrStep);

  return 0;
}