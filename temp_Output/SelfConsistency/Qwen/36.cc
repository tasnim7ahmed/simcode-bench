#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/gnuplot.h"
#include "ns3/simulator.h"
#include "ns3/wifi-utils.h"
#include "ns3/wifi-spectrum-value-helper.h"
#include "ns3/error-rate-model.h"
#include "ns3/yans-error-rate-model.h"
#include "ns3/nist-error-rate-model.h"
#include "ns3/table-based-error-rate-model.h"
#include "ns3/wifi-phy-common.h"
#include "ns3/log.h"
#include "ns3/double.h"

#include <vector>
#include <string>
#include <fstream>
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VhtErrorRateModelsComparison");

void
CalculateFrameSuccessRate (std::string errorModelType,
                           uint32_t frameSize,
                           Gnuplot2dDataset dataset)
{
  Ptr<ErrorRateModel> errorRateModel;

  if (errorModelType == "Yans")
    {
      errorRateModel = CreateObject<YansErrorRateModel>();
    }
  else if (errorModelType == "Nist")
    {
      errorRateModel = CreateObject<NistErrorRateModel>();
    }
  else if (errorModelType == "TableBased")
    {
      errorRateModel = CreateObject<TableBasedErrorRateModel>();
    }
  else
    {
      NS_FATAL_ERROR("Unknown error rate model: " << errorModelType);
    }

  double step = 1.0; // dB
  for (double snrDb = -5.0; snrDb <= 30.0; snrDb += step)
    {
      double ps = 0.0;
      double snr = std::pow (10.0, snrDb / 10.0);

      // Iterate over all VHT MCS values except MCS 9 for 20 MHz
      for (uint8_t mcs = 0; mcs <= 9; ++mcs)
        {
          WifiMode mode = WifiPhy::GetVhtMcs (mcs);
          uint16_t channelWidth = 20;

          if (mode.GetDataRate (channelWidth) == 0 && mcs != 9)
            {
              continue; // Skip invalid MCS values
            }

          if (channelWidth == 20 && mcs == 9)
            {
              continue; // Skip MCS 9 for 20 MHz
            }

          double this_ps = 1.0;
          uint32_t payloadBits = static_cast<uint32_t> (frameSize * 8);
          uint32_t totalBits = payloadBits + 24 * 8; // Assume 24 bytes MAC header

          this_ps *= std::pow ((1.0 - errorRateModel->GetChunkSuccessRate (mode, snr, 8e-6)), totalBits / mode.GetBitsPerSymbol ());
          ps += this_ps;
        }

      double averagePs = ps / 9.0; // Average across valid MCS values
      dataset.Add (snrDb, averagePs);
    }
}

int
main (int argc, char *argv[])
{
  uint32_t frameSize = 1500;
  CommandLine cmd;
  cmd.AddValue ("frameSize", "The size of the frame in bytes", frameSize);
  cmd.Parse (argc, argv);

  std::vector<std::string> models = {"Yans", "Nist", "TableBased"};
  std::vector<Gnuplot2dDataset> datasets;

  for (auto model : models)
    {
      Gnuplot2dDataset dataset (model);
      dataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);
      CalculateFrameSuccessRate (model, frameSize, dataset);
      datasets.push_back (dataset);
    }

  for (size_t i = 0; i < models.size (); ++i)
    {
      Gnuplot plot;
      plot.SetTitle ("Frame Success Rate vs SNR (" + models[i] + ")");
      plot.SetTerminal ("png");
      plot.SetOutputTitle (("vht-" + models[i] + "-fsr").c_str ());
      plot.SetLegend ("SNR (dB)", "Frame Success Rate");
      plot.AddDataset (datasets[i]);

      std::ofstream plotFile (("vht-" + models[i] + "-fsr.plt").c_str ());
      plot.GenerateOutput (plotFile);
      plotFile.close ();
    }

  return 0;
}