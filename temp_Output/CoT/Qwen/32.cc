#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HeErrorRateModelComparison");

class HeErrorRatePlotter : public Object
{
public:
  static TypeId GetTypeId(void);
  HeErrorRatePlotter();
  virtual ~HeErrorRatePlotter();

  void Run(uint32_t frameSize);

private:
  void GeneratePlotData(uint32_t frameSize, Gnuplot2dDataset& datasetNist, Gnuplot2dDataset& datasetYans, Gnuplot2dDataset& datasetTable);
  void CalculateFsrForMcs(double snr, uint8_t mcs, double* fsrNist, double* fsrYans, double* fsrTable, uint32_t frameSize);
};

TypeId
HeErrorRatePlotter::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::HeErrorRatePlotter")
    .SetParent<Object>()
    ;
  return tid;
}

HeErrorRatePlotter::HeErrorRatePlotter()
{
}

HeErrorRatePlotter::~HeErrorRatePlotter()
{
}

void
HeErrorRatePlotter::Run(uint32_t frameSize)
{
  Gnuplot plot;
  plot.SetTitle("Frame Success Rate vs SNR for HE MCS");
  plot.SetTerminal("png");
  plot.SetLegend("SNR (dB)", "Frame Success Rate");
  plot.AppendExtra("set yrange [0:1]");
  plot.AppendExtra("set grid");

  Gnuplot2dDataset datasetNist;
  Gnuplot2dDataset datasetYans;
  Gnuplot2dDataset datasetTable;

  datasetNist.SetStyle(Gnuplot2dDataset::LINES_POINTS);
  datasetYans.SetStyle(Gnuplot2dDataset::LINES_POINTS);
  datasetTable.SetStyle(Gnuplot2dDataset::LINES_POINTS);

  GeneratePlotData(frameSize, datasetNist, datasetYans, datasetTable);

  plot.AddDataset(datasetNist);
  plot.AddDataset(datasetYans);
  plot.AddDataset(datasetTable);

  std::ostringstream filename;
  filename << "he-error-rate-comparison-" << frameSize << "bytes.plt";
  std::ofstream pltFile(filename.str().c_str());
  plot.GenerateOutput(pltFile);
  pltFile.close();
}

void
HeErrorRatePlotter::GeneratePlotData(uint32_t frameSize, Gnuplot2dDataset& datasetNist, Gnuplot2dDataset& datasetYans, Gnuplot2dDataset& datasetTable)
{
  WifiTxVector txVector;
  txVector.SetMode(WifiModeFactory::CreateWifiMode("HeMcs0"));
  txVector.SetChannelWidth(20);
  txVector.SetGuardInterval(3200); // ns

  Ptr<ErrorRateModel> nistModel = CreateObject<NistErrorRateModel>();
  Ptr<ErrorRateModel> yansModel = CreateObject<YansErrorRateModel>();
  Ptr<ErrorRateModel> tableModel = CreateObject<TableBasedErrorRateModel>();

  for (double snrDb = -5; snrDb <= 30; snrDb += 2.5)
    {
      double snr = std::pow(10.0, snrDb / 10.0);
      double avgFsrNist = 0;
      double avgFsrYans = 0;
      double avgFsrTable = 0;
      const uint8_t maxMcs = 11;

      for (uint8_t mcs = 0; mcs <= maxMcs; ++mcs)
        {
          txVector.SetMode(WifiModeFactory::CreateWifiMode(std::string("HeMcs") + std::to_string(mcs)).GetCodeRate());
          double fsrNist, fsrYans, fsrTable;
          CalculateFsrForMcs(snr, mcs, &fsrNist, &fsrYans, &fsrTable, frameSize);
          avgFsrNist += fsrNist;
          avgFsrYans += fsrYans;
          avgFsrTable += fsrTable;
        }

      avgFsrNist /= (maxMcs + 1);
      avgFsrYans /= (maxMcs + 1);
      avgFsrTable /= (maxMcs + 1);

      datasetNist.Add(snrDb, avgFsrNist);
      datasetYans.Add(snrDb, avgFsrYans);
      datasetTable.Add(snrDb, avgFsrTable);
    }
}

void
HeErrorRatePlotter::CalculateFsrForMcs(double snr, uint8_t mcs, double* fsrNist, double* fsrYans, double* fsrTable, uint32_t frameSize)
{
  WifiTxVector txVector;
  txVector.SetMode(WifiModeFactory::CreateWifiMode(std::string("HeMcs") + std::to_string(mcs)));
  txVector.SetChannelWidth(20);
  txVector.SetGuardInterval(3200);
  txVector.SetNss(1);

  *fsrNist = 1.0 - DynamicCast<NistErrorRateModel>(WifiPhy::GetErrorRateModel())->GetChunkSuccessRate(txVector, snr, frameSize * 8).GetValue();
  *fsrYans = 1.0 - DynamicCast<YansErrorRateModel>(WifiPhy::GetErrorRateModel())->GetChunkSuccessRate(txVector, snr, frameSize * 8).GetValue();
  *fsrTable = 1.0 - DynamicCast<TableBasedErrorRateModel>(WifiPhy::GetErrorRateModel())->GetChunkSuccessRate(txVector, snr, frameSize * 8).GetValue();
}

int
main(int argc, char *argv[])
{
  uint32_t frameSize = 1472;

  CommandLine cmd(__FILE__);
  cmd.AddValue("frameSize", "The size of the frame in bytes", frameSize);
  cmd.Parse(argc, argv);

  HeErrorRatePlotter plotter;
  plotter.Run(frameSize);

  return 0;
}