#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include "ns3/yans-error-rate-model.h"
#include "ns3/nist-error-rate-model.h"
#include "ns3/table-error-rate-model.h"
#include "ns3/dsss-modulation.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DsssErrorRateValidation");

int
main (int argc, char *argv[])
{
  LogComponent::SetAttribute ("DsssErrorRateValidation", AttributeValue (BooleanValue (true)));

  uint32_t frameSize = 1000; // bytes
  double simulationTime = 10.0;

  // Configure Gnuplot
  std::string dataFileName = "dsss-error-rate.dat";
  std::string graphicsFileName = "dsss-error-rate.eps";
  std::string plotTitle = "DSSS Frame Success Rate vs. SNR";
  std::string xTitle = "SNR (dB)";
  std::string yTitle = "Frame Success Rate";

  Gnuplot gnuplot (graphicsFileName);
  gnuplot.SetTitle (plotTitle);
  gnuplot.SetLegend ("SNR", "Frame Success Rate");
  gnuplot.SetTerminal ("eps enhanced color");
  gnuplot.AddDataset (dataFileName, "lines");

  // Error Rate Models
  YansErrorRateModel yansErrorRateModel;
  NistErrorRateModel nistErrorRateModel;
  TableErrorRateModel tableErrorRateModel; // Needs a table file.  Omitting loading one for brevity.

  // DSSS Modes
  std::vector<DsssModulation::DsssMode> dsssModes = {
    DsssModulation::DsssMode::DSSS_BASIC,
    DsssModulation::DsssMode::DSSS_1MBPS,
    DsssModulation::DsssMode::DSSS_2MBPS,
    DsssModulation::DsssMode::DSSS_5_5MBPS,
    DsssModulation::DsssMode::DSSS_11MBPS
  };

  // SNR values to test
  std::vector<double> snrValues = {0.0, 2.0, 4.0, 6.0, 8.0, 10.0, 12.0, 14.0, 16.0, 18.0, 20.0};

  // Output file for data
  std::ofstream dataFile;
  dataFile.open (dataFileName.c_str());
  dataFile << "# SNR FrameSuccessRate Mode ErrorRateModel" << std::endl;


  for (auto mode : dsssModes)
    {
      for (double snr : snrValues)
        {
          double successRateYans = yansErrorRateModel.CalculateSuccessRate (snr, mode, frameSize * 8); // bits
          double successRateNist = nistErrorRateModel.CalculateSuccessRate (snr, mode, frameSize * 8); // bits
          //double successRateTable = tableErrorRateModel.CalculateSuccessRate (snr, mode, frameSize * 8); // bits - omitting this one


          // Validate the results (example)
          if (successRateYans < 0.0 || successRateYans > 1.0)
            {
              std::cerr << "Yans Success rate out of range: " << successRateYans << std::endl;
            }
          if (successRateNist < 0.0 || successRateNist > 1.0)
            {
              std::cerr << "Nist Success rate out of range: " << successRateNist << std::endl;
            }


          dataFile << snr << " " << successRateYans << " " << static_cast<int>(mode) << " Yans" << std::endl;
          dataFile << snr << " " << successRateNist << " " << static_cast<int>(mode) << " Nist" << std::endl;
          //dataFile << snr << " " << successRateTable << " " << static_cast<int>(mode) << " Table" << std::endl;

        }
    }

  dataFile.close();

  gnuplot.GenerateOutput ();

  std::cout << "Data written to " << dataFileName << std::endl;
  std::cout << "Gnuplot output written to " << graphicsFileName << std::endl;


  return 0;
}