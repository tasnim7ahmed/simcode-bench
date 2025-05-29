#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

static const uint32_t FRAME_SIZE = 1024; // bytes
static const double SNR_START_DB = 0.0;
static const double SNR_END_DB = 40.0;
static const double SNR_STEP_DB = 1.0;

struct ModelPlot
{
  std::vector<double> snrDb;
  std::vector<double> fsr;
};

double
GetFrameSuccessRate (Ptr<ErrorRateModel> model, WifiTxVector txVector, double snrDb)
{
  double snr = std::pow (10.0, snrDb / 10.0);
  double per = model->GetChunkSuccessRate (txVector, snr, FRAME_SIZE * 8);
  return per;
}

void
PlotModel (std::string modelName, ModelPlot &plot, Gnuplot &gnuplot, uint8_t mcs)
{
  std::ostringstream label;
  label << modelName << " MCS" << unsigned(mcs);
  Gnuplot2dDataset dataset;
  dataset.SetTitle (label.str ());
  dataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  for (uint32_t i = 0; i < plot.snrDb.size (); ++i)
    dataset.Add (plot.snrDb[i], plot.fsr[i]);
  gnuplot.AddDataset (dataset);
}

int
main (int argc, char *argv[])
{
  uint32_t frameSize = FRAME_SIZE;
  double snrStart = SNR_START_DB;
  double snrEnd = SNR_END_DB;
  double snrStep = SNR_STEP_DB;

  CommandLine cmd;
  cmd.AddValue ("frameSize", "Frame size in bytes", frameSize);
  cmd.AddValue ("snrStart", "SNR start value (dB)", snrStart);
  cmd.AddValue ("snrEnd", "SNR end value (dB)", snrEnd);
  cmd.AddValue ("snrStep", "SNR step value (dB)", snrStep);
  cmd.Parse (argc, argv);

  // Prepare Gnuplot
  Gnuplot gnuplotNist ("frame-success-nist.plt");
  Gnuplot gnuplotYans ("frame-success-yans.plt");
  Gnuplot gnuplotTable ("frame-success-table.plt");
  gnuplotNist.SetTitle ("Frame Success Rate vs SNR (Nist Error Model)");
  gnuplotYans.SetTitle ("Frame Success Rate vs SNR (Yans Error Model)");
  gnuplotTable.SetTitle ("Frame Success Rate vs SNR (Table-based Error Model)");
  gnuplotNist.SetLegend ("SNR (dB)", "Frame Success Rate");
  gnuplotYans.SetLegend ("SNR (dB)", "Frame Success Rate");
  gnuplotTable.SetLegend ("SNR (dB)", "Frame Success Rate");

  // Set up WifiTxVector common parameters (EHT MCS reused for HT MCS in this test)
  WifiTxVector txVector;
  txVector.SetNss (1);
  txVector.SetWidth (20);
  txVector.SetPreambleType (WIFI_PREAMBLE_HT_MF);
  txVector.SetGuardInterval (WIFI_GUARD_INTERVAL_NORMAL);

  uint8_t maxHtMcs = 7; // 0-7 for single stream HT
  WifiPhyStandard standard = WIFI_PHY_STANDARD_80211n_2_4GHZ;

  // Error models
  Ptr<YansErrorRateModel> yansModel = CreateObject<YansErrorRateModel> ();
  Ptr<NistErrorRateModel> nistModel = CreateObject<NistErrorRateModel> ();
  Ptr<TableBasedErrorRateModel> tableModel = CreateObject<TableBasedErrorRateModel> ();

  // Iterate for each HT MCS value
  for (uint8_t mcs = 0; mcs <= maxHtMcs; ++mcs)
    {
      ModelPlot nistPlot, yansPlot, tablePlot;
      txVector.SetMode (WifiModeFactory::CreateMcs ("HtMcs" + std::to_string (unsigned (mcs)), standard, 0));
      txVector.SetMcs (mcs);

      for (double snrDb = snrStart; snrDb <= snrEnd; snrDb += snrStep)
        {
          double nistFsr = GetFrameSuccessRate (nistModel, txVector, snrDb);
          double yansFsr = GetFrameSuccessRate (yansModel, txVector, snrDb);
          double tableFsr = GetFrameSuccessRate (tableModel, txVector, snrDb);

          nistPlot.snrDb.push_back (snrDb);
          yansPlot.snrDb.push_back (snrDb);
          tablePlot.snrDb.push_back (snrDb);
          nistPlot.fsr.push_back (nistFsr);
          yansPlot.fsr.push_back (yansFsr);
          tablePlot.fsr.push_back (tableFsr);
        }

      PlotModel ("Nist", nistPlot, gnuplotNist, mcs);
      PlotModel ("Yans", yansPlot, gnuplotYans, mcs);
      PlotModel ("Table", tablePlot, gnuplotTable, mcs);
    }

  std::ofstream nistPlotFile ("frame-success-nist.plt");
  gnuplotNist.GenerateOutput (nistPlotFile);
  nistPlotFile.close ();

  std::ofstream yansPlotFile ("frame-success-yans.plt");
  gnuplotYans.GenerateOutput (yansPlotFile);
  yansPlotFile.close ();

  std::ofstream tablePlotFile ("frame-success-table.plt");
  gnuplotTable.GenerateOutput (tablePlotFile);
  tablePlotFile.close ();

  NS_LOG_UNCOND ("Gnuplot script and data files generated: frame-success-nist.plt, frame-success-yans.plt, frame-success-table.plt");
  return 0;
}