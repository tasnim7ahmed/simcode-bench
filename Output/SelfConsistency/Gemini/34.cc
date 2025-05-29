#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/command-line.h"
#include "ns3/gnuplot.h"
#include "ns3/he-error-rate-model.h"
#include "ns3/yans-error-rate-model.h"
#include "ns3/nist-error-rate-model.h"
#include "ns3/table-error-rate-model.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HeErrorRateModelsValidation");

int main (int argc, char *argv[])
{
  // Enable logging for the relevant modules
  LogComponentEnable ("HeErrorRateModelsValidation", LOG_LEVEL_INFO);

  uint32_t frameSize = 12000; // bits
  double startSnr = 0.0;       // dB
  double stopSnr = 25.0;        // dB
  double snrStep = 1.0;        // dB
  std::string prefixFile = "he-error-rate-validation";

  CommandLine cmd;
  cmd.AddValue ("frameSize", "Frame size in bits", frameSize);
  cmd.AddValue ("startSnr", "Start SNR in dB", startSnr);
  cmd.AddValue ("stopSnr", "Stop SNR in dB", stopSnr);
  cmd.AddValue ("snrStep", "SNR step in dB", snrStep);
  cmd.AddValue ("prefix", "Prefix for output files", prefixFile);
  cmd.Parse (argc, argv);

  // Configure the simulation
  Config::SetDefault ("ns3::WifiNetDevice::SupportedPhys", StringValue ("He80MHz")) ;

  // Create the error rate models
  HeErrorRateModel heErrorRateModel;
  YansErrorRateModel yansErrorRateModel;
  NistErrorRateModel nistErrorRateModel;
  TableErrorRateModel tableErrorRateModel;

  // Configure a basic spectrum channel
  Ptr<SpectrumChannel> spectrumChannel = CreateObject<SpectrumChannel> ();
  Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel> ();
  spectrumChannel->SetPropagationDelayModel (delayModel);
  Ptr<FixedRssLossModel> lossModel = CreateObject<FixedRssLossModel> ();
  spectrumChannel->AddPropagationLossModel (lossModel);

  // Iterate over the MCS values
  for (uint32_t mcs = 0; mcs <= 11; ++mcs)
    {
      // Open output files for Gnuplot data
      std::ofstream nistOutput;
      std::ofstream yansOutput;
      std::ofstream tableOutput;
      std::ofstream heOutput;

      nistOutput.open (prefixFile + "-nist-mcs" + std::to_string (mcs) + ".dat");
      yansOutput.open (prefixFile + "-yans-mcs" + std::to_string (mcs) + ".dat");
      tableOutput.open (prefixFile + "-table-mcs" + std::to_string (mcs) + ".dat");
      heOutput.open (prefixFile + "-he-mcs" + std::to_string (mcs) + ".dat");

      nistOutput << "# SNR FrameSuccessRate" << std::endl;
      yansOutput << "# SNR FrameSuccessRate" << std::endl;
      tableOutput << "# SNR FrameSuccessRate" << std::endl;
      heOutput << "# SNR FrameSuccessRate" << std::endl;

      // Iterate over the SNR values
      for (double snr = startSnr; snr <= stopSnr; snr += snrStep)
        {
          // Compute the frame success rate for each model
          double nistFrameSuccessRate = nistErrorRateModel.GetSuccessRate (snr, mcs, frameSize);
          double yansFrameSuccessRate = yansErrorRateModel.GetSuccessRate (snr, mcs, frameSize);
          double tableFrameSuccessRate = tableErrorRateModel.GetSuccessRate (snr, mcs, frameSize);
          double heFrameSuccessRate = heErrorRateModel.GetSuccessRate (snr, mcs, frameSize);

          // Output the results to the Gnuplot data files
          nistOutput << std::fixed << std::setprecision (2) << snr << " " << nistFrameSuccessRate << std::endl;
          yansOutput << std::fixed << std::setprecision (2) << snr << " " << yansFrameSuccessRate << std::endl;
          tableOutput << std::fixed << std::setprecision (2) << snr << " " << tableFrameSuccessRate << std::endl;
          heOutput << std::fixed << std::setprecision (2) << snr << " " << heFrameSuccessRate << std::endl;
        }

      // Close the output files
      nistOutput.close ();
      yansOutput.close ();
      tableOutput.close ();
      heOutput.close ();

      // Generate Gnuplot script
      std::ofstream gnuplotScript;
      gnuplotScript.open (prefixFile + "-mcs" + std::to_string (mcs) + ".gp");
      gnuplotScript << "set terminal png size 1024,768" << std::endl;
      gnuplotScript << "set output '" << prefixFile + "-mcs" + std::to_string (mcs) << ".png'" << std::endl;
      gnuplotScript << "set title 'Frame Success Rate vs. SNR (MCS " << mcs << ")'" << std::endl;
      gnuplotScript << "set xlabel 'SNR (dB)'" << std::endl;
      gnuplotScript << "set ylabel 'Frame Success Rate'" << std::endl;
      gnuplotScript << "set xrange [" << startSnr << ":" << stopSnr << "]" << std::endl;
      gnuplotScript << "set yrange [0:1]" << std::endl;
      gnuplotScript << "plot '" << prefixFile + "-nist-mcs" + std::to_string (mcs) << ".dat' using 1:2 with lines title 'NIST',"
                    << "     '" << prefixFile + "-yans-mcs" + std::to_string (mcs) << ".dat' using 1:2 with lines title 'YANS',"
                    << "     '" << prefixFile + "-table-mcs" + std::to_string (mcs) << ".dat' using 1:2 with lines title 'Table',"
                    << "     '" << prefixFile + "-he-mcs" + std::to_string (mcs) << ".dat' using 1:2 with lines title 'HE'" << std::endl;
      gnuplotScript.close ();

      // Execute Gnuplot (optional)
      std::string command = "gnuplot " + prefixFile + "-mcs" + std::to_string (mcs) + ".gp";
      int result = system (command.c_str ());
      if (result != 0)
        {
          std::cerr << "Error executing gnuplot.  Please ensure gnuplot is installed." << std::endl;
        }
    }

  Simulator::Destroy ();
  return 0;
}