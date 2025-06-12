#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HeErrorRateModelComparison");

class HeErrorRateModelTest : public Object
{
public:
  static TypeId GetTypeId(void);
  HeErrorRateModelTest();
  virtual ~HeErrorRateModelTest();

  void Run(uint32_t frameSize, const std::string& outputFileName);

private:
  void GeneratePlot(const std::string& filename);

  double CalculateFsr(ErrorRateModel* errorModel, WifiMode mode, double snr, uint32_t size);
};

TypeId
HeErrorRateModelTest::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::HeErrorRateModelTest")
                          .SetParent<Object>()
                          .AddConstructor<HeErrorRateModelTest>();
  return tid;
}

HeErrorRateModelTest::HeErrorRateModelTest()
{
}

HeErrorRateModelTest::~HeErrorRateModelTest()
{
}

double
HeErrorRateModelTest::CalculateFsr(ErrorRateModel* errorModel, WifiMode mode, double snr, uint32_t size)
{
  uint32_t payloadBits = size * 8;
  return 1.0 - errorModel->GetChunkSuccessRate(mode, snr, payloadBits);
}

void
HeErrorRateModelTest::Run(uint32_t frameSize, const std::string& outputFileName)
{
  Gnuplot plot(outputFileName + ".png");
  plot.SetTitle("Frame Success Rate vs SNR for HE MCS (All Error Models)");
  plot.SetTerminal("png");
  plot.SetLegend("SNR (dB)", "Frame Success Rate");

  Gnuplot2dDataset datasetNist;
  datasetNist.SetStyle(Gnuplot2dDataset::LINES_POINTS);
  datasetNist.SetTitle("NIST");

  Gnuplot2dDataset datasetYans;
  datasetYans.SetStyle(Gnuplot2dDataset::LINES_POINTS);
  datasetYans.SetTitle("YANS");

  Gnuplot2dDataset datasetTable;
  datasetTable.SetStyle(Gnuplot2dDataset::LINES_POINTS);
  datasetTable.SetTitle("Table-Based");

  // Setup NIST model
  Ptr<NistErrorRateModel> nistModel = CreateObject<NistErrorRateModel>();
  // Setup YANS model
  Ptr<YansErrorRateModel> yansModel = CreateObject<YansErrorRateModel>();
  // Setup Table-based model
  Ptr<TableBasedErrorRateModel> tableModel = CreateObject<TableBasedErrorRateModel>();

  // Iterate over all HE MCS values
  for (uint8_t mcs = 0; mcs <= 11; ++mcs)
    {
      WifiMode mode = WifiPhy::GetHeMcs(mcs);

      if (!mode.IsDefined())
        continue;

      double snrStart = 0.0;
      double snrEnd = 30.0;
      double snrStep = 2.0;

      for (double snrDb = snrStart; snrDb <= snrEnd; snrDb += snrStep)
        {
          double snrLinear = std::pow(10.0, snrDb / 10.0);

          datasetNist.Add(snrDb,
                          CalculateFsr(PeekPointer(nistModel), mode, snrLinear, frameSize));
          datasetYans.Add(snrDb,
                          CalculateFsr(PeekPointer(yansModel), mode, snrLinear, frameSize));
          datasetTable.Add(snrDb,
                           CalculateFsr(PeekPointer(tableModel), mode, snrLinear, frameSize));
        }

      // Save individual MCS plot
      std::ostringstream oss;
      oss << outputFileName << "-mcs" << static_cast<int>(mcs) << ".png";

      Gnuplot mcsPlot(oss.str.c_str());
      mcsPlot.SetTitle("Frame Success Rate vs SNR for HE MCS" + std::to_string(mcs));
      mcsPlot.SetTerminal("png");
      mcsPlot.SetLegend("SNR (dB)", "Frame Success Rate");

      Gnuplot2dDataset mcsDatasetNist;
      mcsDatasetNist.SetStyle(Gnuplot2dDataset::LINES_POINTS);
      mcsDatasetNist.SetTitle("NIST");

      Gnuplot2dDataset mcsDatasetYans;
      mcsDatasetYans.SetStyle(Gnuplot2dDataset::LINES_POINTS);
      mcsDatasetYans.SetTitle("YANS");

      Gnuplot2dDataset mcsDatasetTable;
      mcsDatasetTable.SetStyle(Gnuplot2dDataset::LINES_POINTS);
      mcsDatasetTable.SetTitle("Table-Based");

      for (double snrDb = snrStart; snrDb <= snrEnd; snrDb += snrStep)
        {
          double snrLinear = std::pow(10.0, snrDb / 10.0);

          mcsDatasetNist.Add(
              snrDb, CalculateFsr(PeekPointer(nistModel), mode, snrLinear, frameSize));
          mcsDatasetYans.Add(
              snrDb, CalculateFsr(PeekPointer(yansModel), mode, snrLinear, frameSize));
          mcsDatasetTable.Add(
              snrDb, CalculateFsr(PeekPointer(tableModel), mode, snrLinear, frameSize));
        }

      mcsPlot.AddDataset(mcsDatasetNist);
      mcsPlot.AddDataset(mcsDatasetYans);
      mcsPlot.AddDataset(mcsDatasetTable);
      std::ofstream mcsPlotFile((outputFileName + "-mcs" + std::to_string(mcs) + ".plt").c_str());
      mcsPlot.GenerateOutput(mcsPlotFile);
      mcsPlotFile.close();
    }

  plot.AddDataset(datasetNist);
  plot.AddDataset(datasetYans);
  plot.AddDataset(datasetTable);

  std::ofstream plotFile((outputFileName + ".plt").c_str());
  plot.GenerateOutput(plotFile);
  plotFile.close();
}

int
main(int argc, char* argv[])
{
  uint32_t frameSize = 1500;
  std::string outputFileName = "he_error_rate_comparison";

  CommandLine cmd(__FILE__);
  cmd.AddValue("frameSize", "The size of the frame in bytes", frameSize);
  cmd.AddValue("output", "Base name for output files", outputFileName);
  cmd.Parse(argc, argv);

  HeErrorRateModelTest test;
  test.Run(frameSize, outputFileName);

  return 0;
}