#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ErrorModelComparison");

class ErrorModelComparison {
public:
  void Run (std::string frameSizeStr, std::string errorModelType);
};

void
ErrorModelComparison::Run (std::string frameSizeStr, std::string errorModelType)
{
  uint32_t frameSize = std::stoi(frameSizeStr);
  double snrStart = -5;
  double snrEnd = 30;
  double snrStep = 1;
  uint32_t totalPackets = 1000;

  Gnuplot gnuplot ("frame-success-rate-vs-snr.png");
  gnuplot.SetTitle("Frame Success Rate vs SNR for Different Error Models");
  gnuplot.SetTerminal("png");
  gnuplot.SetLegend("SNR (dB)", "Frame Success Rate");

  std::vector<std::string> modes;
  modes.push_back("OfdmRate6_5MbpsBW20MHz");
  modes.push_back("OfdmRate13MbpsBW20MHzShGi");
  modes.push_back("OfdmRate19_5MbpsBW20MHz");
  modes.push_back("OfdmRate26MbpsBW20MHzShGi");
  modes.push_back("OfdmRate39MbpsBW20MHz");
  modes.push_back("OfdmRate52MbpsBW20MHzShGi");
  modes.push_back("OfdmRate58_5MbpsBW20MHz");
  modes.push_back("OfdmRate65MbpsBW20MHzShGi");

  for (auto mode : modes)
    {
      WifiTxVector txVector;
      txVector.SetMode(WifiMode(mode));
      txVector.SetGuardInterval(800); // nanoseconds

      Gnuplot2dDataset dataset (mode);

      for (double snrDb = snrStart; snrDb <= snrEnd; snrDb += snrStep)
        {
          double snr = std::pow(10.0, snrDb / 10.0);
          uint32_t success = 0;

          for (uint32_t i = 0; i < totalPackets; ++i)
            {
              double ber = 0.0;

              if (errorModelType == "Nist")
                {
                  NistErrorRateModel nist;
                  ber = nist.GetChunkSuccessRate (txVector, 8 * frameSize, snr);
                }
              else if (errorModelType == "Yans")
                {
                  YansErrorRateModel yans;
                  ber = yans.GetChunkSuccessRate (txVector, 8 * frameSize, snr);
                }
              else if (errorModelType == "Table")
                {
                  TableBasedErrorRateModel table;
                  ber = table.GetChunkSuccessRate (txVector, 8 * frameSize, snr);
                }

              if (ber > 0.0)
                {
                  success++;
                }
            }

          double fsr = static_cast<double>(success) / totalPackets;
          dataset.Add (snrDb, fsr);
        }

      gnuplot.AddDataset(dataset);
    }

  std::ofstream plotFile ("frame-success-rate-vs-snr.plt");
  gnuplot.GenerateOutput(plotFile);
}

int
main (int argc, char *argv[])
{
  std::string frameSizeStr = "1472"; // default size in bytes
  std::string errorModelType = "Nist";

  CommandLine cmd (__FILE__);
  cmd.AddValue ("frameSize", "The size of the frame to send in bytes", frameSizeStr);
  cmd.AddValue ("errorModel", "Error model type: Nist, Yans, or Table", errorModelType);
  cmd.Parse (argc, argv);

  ErrorModelComparison comparison;
  comparison.Run (frameSizeStr, errorModelType);

  return 0;
}