#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ht-wifi-mac.h"
#include "ns3/gnuplot.h"
#include "ns3/yans-error-rate-model.h"
#include "ns3/nist-error-rate-model.h"
#include "ns3/table-error-rate-model.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HtErrorRateModelValidation");

int main (int argc, char *argv[])
{
  // Enable logging for debugging purposes
  // LogComponentEnable ("HtErrorRateModelValidation", LOG_LEVEL_ALL);

  // Simulation parameters
  double simulationTime = 10.0;  // seconds
  uint32_t frameSize = 1000;     // bytes
  uint32_t numFrames = 1000;     // Number of frames to transmit per SNR
  double startSnr = 5.0;         // dB
  double endSnr = 25.0;           // dB
  double snrStep = 1.0;          // dB
  std::string prefix = "ht-error-rate-"; // Prefix for output files

  // Configure command line arguments
  CommandLine cmd;
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("frameSize", "Frame size in bytes", frameSize);
  cmd.AddValue ("numFrames", "Number of frames to transmit per SNR", numFrames);
  cmd.AddValue ("startSnr", "Start SNR in dB", startSnr);
  cmd.AddValue ("endSnr", "End SNR in dB", endSnr);
  cmd.AddValue ("snrStep", "SNR step in dB", snrStep);
  cmd.AddValue ("prefix", "Prefix for output files", prefix);
  cmd.Parse (argc, argv);

  // Create output directory
  std::string dir = "results";
  std::string command = "mkdir -p " + dir;
  if (system (command.c_str ()) != 0)
  {
    std::cout << "Error creating directory" << std::endl;
    return 1;
  }


  // Loop through HT MCS values
  for (int mcs = 0; mcs <= 7; ++mcs)
  {
    // Create output files for each model and MCS value
    std::ofstream yansOutput, nistOutput, tableOutput;
    yansOutput.open (dir + "/" + prefix + "yans-mcs" + std::to_string (mcs) + ".dat");
    nistOutput.open (dir + "/" + prefix + "nist-mcs" + std::to_string (mcs) + ".dat");
    tableOutput.open (dir + "/" + prefix + "table-mcs" + std::to_string (mcs) + ".dat");

    // Configure Wi-Fi PHY parameters
    WifiMode wifiMode = WifiModeFactory::GetHtMcs (mcs);
    double dataRate = wifiMode.GetDataRate ();
    double symbolDuration = wifiMode.GetSymbolPeriod ();
    double sifs = 16e-6; // 802.11a-1999
    double slot = 9e-6;
    double cwMin = 15;
    double cwMax = 1023;
    double ackTimeout = sifs + 2 * slot + ((double)(frameSize * 8) / dataRate);

    // Loop through SNR values
    for (double snr = startSnr; snr <= endSnr; snr += snrStep)
    {
      // Configure YANS error rate model
      YansErrorRateModel yansErrorModel;
      yansErrorModel.SetDefaultHtWifiParameters ();

      // Configure NIST error rate model
      NistErrorRateModel nistErrorModel;
      nistErrorModel.SetHtMcs (mcs);

      // Configure Table error rate model
      TableErrorRateModel tableErrorModel;

      // Calculate frame success rate for each model
      double yansSuccesses = 0;
      double nistSuccesses = 0;
      double tableSuccesses = 0;

      for (uint32_t i = 0; i < numFrames; ++i)
      {
        // YANS
        double ber = yansErrorModel.GetBerFromSnr (snr, wifiMode);
        double per = 1 - pow ((1 - ber), (frameSize * 8)); // PER = 1 - (1 - BER)^(frame size in bits)
        if (Simulator::Now ().GetSeconds () <= simulationTime && per < 1 - DBL_EPSILON)
        {
          if (rand () / (double) RAND_MAX > per)
          {
            yansSuccesses++;
          }
        }

        // NIST
        if (nistErrorModel.AssignStreams (1)) // Assign a stream to NIST to make it usable
        {
          if (nistErrorModel.IsFrameCorrupt (snr, frameSize * 8)) // Frame size in bits for NIST
          {
            // Frame is corrupt, do nothing
          } else
          {
            nistSuccesses++;
          }
        }

        // Table
        if (tableErrorModel.AssignStreams (1))
        {
          double perTable = tableErrorModel.GetPer (snr, wifiMode, frameSize * 8); // Frame size in bits for Table
          if (rand () / (double) RAND_MAX > perTable)
          {
              tableSuccesses++;
          }
        }
      }

      // Calculate frame success rate
      double yansFrameSuccessRate = yansSuccesses / numFrames;
      double nistFrameSuccessRate = nistSuccesses / numFrames;
      double tableFrameSuccessRate = tableSuccesses / numFrames;

      // Output results to files
      yansOutput << snr << " " << yansFrameSuccessRate << std::endl;
      nistOutput << snr << " " << nistFrameSuccessRate << std::endl;
      tableOutput << snr << " " << tableFrameSuccessRate << std::endl;
    }

    // Close output files
    yansOutput.close ();
    nistOutput.close ();
    tableOutput.close ();

    // Create Gnuplot script
    Gnuplot gnuplot;
    gnuplot.SetTitle ("Frame Success Rate vs. SNR (MCS " + std::to_string (mcs) + ")");
    gnuplot.SetLegend ("YANS", "NIST", "Table");
    gnuplot.SetTerminal ("png");
    gnuplot.SetOutput (dir + "/" + prefix + "mcs" + std::to_string (mcs) + ".png");
    gnuplot.AddDataset (dir + "/" + prefix + "yans-mcs" + std::to_string (mcs) + ".dat", "lines");
    gnuplot.AddDataset (dir + "/" + prefix + "nist-mcs" + std::to_string (mcs) + ".dat", "lines");
    gnuplot.AddDataset (dir + "/" + prefix + "table-mcs" + std::to_string (mcs) + ".dat", "lines");
    gnuplot.GenerateOutput ();
  }

  Simulator::Destroy ();
  return 0;
}