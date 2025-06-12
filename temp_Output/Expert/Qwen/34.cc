#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HeErrorRateModelsComparison");

class HeErrorRateComparison : public Object
{
public:
  static TypeId GetTypeId (void);
  HeErrorRateComparison ();
  virtual ~HeErrorRateComparison ();

  void RunSimulation (uint32_t frameSize);

private:
  void SetupSimulation (uint32_t frameSize, double snrDb);
  double CalculateFsr (Ptr<ErrorRateModel> errorRateModel, WifiMode mode, uint32_t frameSize, double snrDb);
};

TypeId
HeErrorRateComparison::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HeErrorRateComparison")
    .SetParent<Object> ()
    .SetGroupName ("Wifi")
  ;
  return tid;
}

HeErrorRateComparison::HeErrorRateComparison ()
  : Object ()
{
}

HeErrorRateComparison::~HeErrorRateComparison ()
{
}

double
HeErrorRateComparison::CalculateFsr (Ptr<ErrorRateModel> errorRateModel, WifiMode mode, uint32_t frameSize, double snrDb)
{
  double ber = errorRateModel->GetBitErrorRate (mode, snrDb, frameSize * 8);
  double fsr = std::pow ((1.0 - ber), (frameSize * 8));
  return fsr;
}

void
HeErrorRateComparison::RunSimulation (uint32_t frameSize)
{
  Gnuplot2dDataset nistDataset ("NIST");
  nistDataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);
  Gnuplot2dDataset yansDataset ("YANS");
  yansDataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);
  Gnuplot2dDataset tableDataset ("Table-Based");
  tableDataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  Ptr<NistErrorRateModel> nistModel = CreateObject<NistErrorRateModel> ();
  Ptr<YansErrorRateModel> yansModel = CreateObject<YansErrorRateModel> ();
  Ptr<TableBasedErrorRateModel> tableModel = CreateObject<TableBasedErrorRateModel> ();

  for (double snrDb = 0; snrDb <= 30; snrDb += 2)
    {
      double nistFsrSum = 0;
      double yansFsrSum = 0;
      double tableFsrSum = 0;
      uint32_t mcsCount = 0;

      for (uint8_t mcs = 0; mcs <= 11; ++mcs) // HE MCS 0-11
        {
          WifiMode mode = WifiPhy::GetHtMcs (mcs); // HT used as base for HE MCS index

          nistFsrSum += CalculateFsr (nistModel, mode, frameSize, snrDb);
          yansFsrSum += CalculateFsr (yansModel, mode, frameSize, snrDb);
          tableFsrSum += CalculateFsr (tableModel, mode, frameSize, snrDb);
          ++mcsCount;
        }

      double avgNistFsr = nistFsrSum / mcsCount;
      double avgYansFsr = yansFsrSum / mcsCount;
      double avgTableFsr = tableFsrSum / mcsCount;

      nistDataset.Add (snrDb, avgNistFsr);
      yansDataset.Add (snrDb, avgYansFsr);
      tableDataset.Add (snrDb, avgTableFsr);
    }

  Gnuplot gnuplot;
  gnuplot.SetTitle ("Frame Success Rate vs SNR for HE Rates");
  gnuplot.SetTerminal ("png");
  gnuplot.SetLegend ("SNR (dB)", "Frame Success Rate");
  gnuplot.AddDataset (nistDataset);
  gnuplot.AddDataset (yansDataset);
  gnuplot.AddDataset (tableDataset);
  std::ofstream plotFile ("he-error-rate-comparison.plt");
  gnuplot.GenerateOutput (plotFile);
  plotFile.close ();

  std::ofstream dataFile ("he-fsr-results.dat");
  dataFile << "# SNR(dB)\tAvgFSR(NIST)\tAvgFSR(YANS)\tAvgFSR(Table)" << std::endl;
  for (size_t i = 0; i < nistDataset.GetNumberofDataPoints (); ++i)
    {
      double x = nistDataset.GetPoint (i).x;
      dataFile << x << "\t" << nistDataset.GetPoint (i).y << "\t"
               << yansDataset.GetPoint (i).y << "\t"
               << tableDataset.GetPoint (i).y << std::endl;
    }
  dataFile.close ();
}

int
main (int argc, char *argv[])
{
  uint32_t frameSize = 1500;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("frameSize", "The size of the frame in bytes", frameSize);
  cmd.Parse (argc, argv);

  HeErrorRateComparison comparison;
  comparison.RunSimulation (frameSize);

  Simulator::Destroy ();
  return 0;
}