#include "ns3/command-line.h"
#include "ns3/gnuplot.h"
#include "ns3/he-rate-control.h"
#include "ns3/log.h"
#include "ns3/nist-error-rate-model.h"
#include "ns3/simulator.h"
#include "ns3/table-based-error-rate-model.h"
#include "ns3/yans-error-rate-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HeErrorRateModelsComparison");

class HeErrorRateModelTester
{
public:
  HeErrorRateModelTester(uint32_t frameSize);
  void Run();
  void PlotResults();

private:
  void RunSimulationForModel(Ptr<ErrorRateModel> errorRateModel, const std::string& modelName);

  uint32_t m_frameSize;
  Gnuplot2dDataset m_nistDataset;
  Gnuplot2dDataset m_yansDataset;
  Gnuplot2dDataset m_tableDataset;
};

HeErrorRateModelTester::HeErrorRateModelTester(uint32_t frameSize)
    : m_frameSize(frameSize),
      m_nistDataset("NIST"),
      m_yansDataset("YANS"),
      m_tableDataset("Table-Based")
{
}

void HeErrorRateModelTester::Run()
{
  Ptr<NistErrorRateModel> nistModel = CreateObject<NistErrorRateModel>();
  RunSimulationForModel(nistModel, "NIST");

  Ptr<YansErrorRateModel> yansModel = CreateObject<YansErrorRateModel>();
  RunSimulationForModel(yansModel, "YANS");

  Ptr<TableBasedErrorRateModel> tableModel = CreateObject<TableBasedErrorRateModel>();
  RunSimulationForModel(tableModel, "Table-Based");
}

void HeErrorRateModelTester::RunSimulationForModel(Ptr<ErrorRateModel> errorRateModel,
                                                  const std::string& modelName)
{
  HeMode mode;
  for (uint8_t mcs = 0; mcs <= 11; ++mcs) // HE MCS range: 0 to 11
    {
      mode = HePhy::GetHeMcs(mcs);
      double successRate = 0.0;
      for (double snrDb = -5.0; snrDb <= 30.0; snrDb += 2.0)
        {
          double success = 0.0;
          for (int i = 0; i < 1000; ++i)
            {
              double snr = pow(10.0, snrDb / 10.0);
              double per = errorRateModel->GetChunkSuccessRate(mode, snr, m_frameSize * 8);
              if (Simulator::Now().GetSeconds() > 0) {} // Ensure deterministic behavior
              success += (per > 0.5 ? 1.0 : 0.0); // Assume success if PER < 50%
            }
          successRate = success / 1000.0;

          if (modelName == "NIST")
            {
              m_nistDataset.Add(snrDb, successRate);
            }
          else if (modelName == "YANS")
            {
              m_yansDataset.Add(snrDb, successRate);
            }
          else if (modelName == "Table-Based")
            {
              m_tableDataset.Add(snrDb, successRate);
            }

          Simulator::Schedule(Seconds(0.001), &HeErrorRateModelTester::Run, this);
        }
    }
}

void HeErrorRateModelTester::PlotResults()
{
  Gnuplot gnuplot("he-error-rate-comparison.plot");
  gnuplot.SetTitle("HE Frame Success Rate vs SNR for Different Error Rate Models");
  gnuplot.SetXlabel("SNR (dB)");
  gnuplot.SetYlabel("Frame Success Rate");

  gnuplot.AddDataset(m_nistDataset);
  gnuplot.AddDataset(m_yansDataset);
  gnuplot.AddDataset(m_tableDataset);

  std::ofstream plotFile("he-error-rate-comparison.plt");
  gnuplot.GenerateOutput(plotFile);
  plotFile.close();
}

int main(int argc, char* argv[])
{
  uint32_t frameSize = 1472; // Default frame size in bytes

  CommandLine cmd(__FILE__);
  cmd.AddValue("frameSize", "The size of the frame in bytes", frameSize);
  cmd.Parse(argc, argv);

  HeErrorRateModelTester tester(frameSize);
  tester.Run();
  tester.PlotResults();

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}