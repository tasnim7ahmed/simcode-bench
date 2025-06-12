#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/propagation-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HeErrorRateModels");

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t packetSize = 1472;
  double minSnr = 5.0;
  double maxSnr = 25.0;
  double snrStep = 1.0;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("verbose", "Tell application to log if possible", verbose);
  cmd.AddValue ("packetSize", "Size of packet sent in bytes", packetSize);
  cmd.AddValue ("minSnr", "Minimum SNR value (dB)", minSnr);
  cmd.AddValue ("maxSnr", "Maximum SNR value (dB)", maxSnr);
  cmd.AddValue ("snrStep", "SNR step (dB)", snrStep);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("HeErrorRateModels", LOG_LEVEL_ALL);
    }

  std::vector<std::string> errorRateModelNames = {"NistErrorRateModel", "YansErrorRateModel", "HeErrorRateModel"};
  std::vector<std::string> mcsNames = {"HeMcs0", "HeMcs1", "HeMcs2", "HeMcs3", "HeMcs4", "HeMcs5", "HeMcs6", "HeMcs7", "HeMcs8", "HeMcs9", "HeMcs10", "HeMcs11"};

  std::map<std::string, std::ofstream> dataFiles;
  std::map<std::string, Gnuplot> gnuplotFiles;

  for (const auto& modelName : errorRateModelNames) {
      for (const auto& mcsName : mcsNames) {
          std::string fileName = modelName + "_" + mcsName + ".dat";
          dataFiles[modelName + "_" + mcsName] = std::ofstream (fileName.c_str());
          dataFiles[modelName + "_" + mcsName] << "# SNR FrameSuccessRate" << std::endl;

          Gnuplot gnuplot (modelName + "_" + mcsName + ".png");
          gnuplot.SetTitle (modelName + " - " + mcsName);
          gnuplot.SetLegend ("SNR (dB)", "Frame Success Rate");
          gnuplot.AddDataset (fileName, "lines");
          gnuplotFiles[modelName + "_" + mcsName] = gnuplot;
      }
  }


  for (double snr = minSnr; snr <= maxSnr; snr += snrStep)
    {
      for (const auto& modelName : errorRateModelNames)
        {
          for (const auto& mcsName : mcsNames)
            {
              TypeId errorRateModelTypeId = TypeId::LookupByName (modelName);
              Ptr<ErrorRateModel> errorRateModel = DynamicCast<ErrorRateModel> (TypeId::Construct (errorRateModelTypeId));
              WifiMode wifiMode;
              try {
                wifiMode = WifiModeFactory::GetHeMcsFromString (mcsName);
              } catch (const std::runtime_error& e) {
                  std::cerr << "Error getting WifiMode for " << mcsName << ": " << e.what() << std::endl;
                  continue;
              }


              double sinr = pow (10.0, snr / 10.0);

              double ber = errorRateModel->CalculateBer (sinr, wifiMode, packetSize);
              double per = 1 - pow ((1 - ber), (packetSize * 8));
              double frameSuccessRate = 1.0 - per;

              dataFiles[modelName + "_" + mcsName] << std::fixed << std::setprecision(2) << snr << " " << frameSuccessRate << std::endl;
            }
        }
    }


  for (const auto& entry : dataFiles) {
      entry.second.close();
  }

  for (const auto& entry : gnuplotFiles) {
      entry.second.GenerateOutput();
  }

  return 0;
}