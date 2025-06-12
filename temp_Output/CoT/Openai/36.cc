#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <string>
#include <fstream>
#include <iomanip>
#include <limits>

using namespace ns3;

static std::vector<std::string> GetVhtMcsModes ()
{
  WifiHelper wifi;
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  WifiMacHelper wifiMac;
  wifi.SetStandard (WIFI_STANDARD_80211ac);

  NodeContainer nodes;
  nodes.Create (2);
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice> (devices.Get (0));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));

  std::vector<std::string> modeList;
  WifiModeFactory *factory = WifiModeFactory::GetFactory ();
  std::vector<std::string> allModes = factory->GetModeList ();

  for (std::vector<std::string>::const_iterator it = allModes.begin (); it != allModes.end (); ++it)
    {
      std::string mode = *it;
      // VHT for 20MHz, MCS 0..8
      if (mode.find ("VhtMcs") != std::string::npos
          && mode.find ("HtMcs") == std::string::npos
          && mode.find ("HeMcs") == std::string::npos
          && mode.find ("EhtMcs") == std::string::npos
          && mode.find ("20MHz") != std::string::npos)
        {
          // Parse MCS number
          size_t p1 = mode.find ("Mcs");
          size_t p2 = mode.find ("-", p1);
          std::string nstr = mode.substr (p1 + 3, p2 - (p1 + 3));
          uint32_t mcs = std::stoi (nstr);
          // Skip MCS 9
          if (mcs < 9)
            {
              modeList.push_back (mode);
            }
        }
    }
  return modeList;
}

enum ErrorModelType
{
  NIST = 0,
  YANS = 1,
  TABLE = 2
};

Ptr<ErrorRateModel>
CreateErrorRateModel (ErrorModelType type)
{
  switch (type)
    {
    case NIST:
      return CreateObject<NistErrorRateModel> ();
    case YANS:
      return CreateObject<YansErrorRateModel> ();
    case TABLE:
      return CreateObject<TableBasedErrorRateModel> ();
    }
  return 0;
}

std::string
ErrorModelName (ErrorModelType type)
{
  switch (type)
    {
    case NIST:
      return "NIST";
    case YANS:
      return "YANS";
    case TABLE:
      return "TABLE";
    }
  return "";
}

double
SnrDbToRatio (double db)
{
  return std::pow (10.0, db / 10.0);
}

int
main (int argc, char *argv[])
{
  uint32_t frameSize = 1200;

  CommandLine cmd;
  cmd.AddValue ("frameSize", "Set the frame size (bytes)", frameSize);
  cmd.Parse (argc, argv);

  std::vector<std::string> vhtModes = GetVhtMcsModes ();

  std::vector<ErrorModelType> models = {NIST, YANS, TABLE};

  const double snrStart = -5.0;
  const double snrEnd = 30.0;
  const double snrStep = 0.5;

  std::vector<double> snrList;
  for (double snr = snrStart; snr <= snrEnd + 0.0001; snr += snrStep)
    {
      snrList.push_back (snr);
    }

  for (ErrorModelType emType : models)
    {
      std::string modelName = ErrorModelName (emType);
      std::string plotFile = "vht-fsr-vs-snr-" + modelName + ".plt";
      std::string dataFile = "vht-fsr-vs-snr-" + modelName + ".dat";

      Gnuplot gnuPlot (plotFile);
      std::vector<Gnuplot2dDataset> datasets;

      for (std::string modeStr : vhtModes)
        {
          Ptr<ErrorRateModel> erm = CreateErrorRateModel (emType);

          Gnuplot2dDataset dataset (modeStr, Gnuplot2dDataset::LINES_POINTS);

          for (double snrDb : snrList)
            {
              double snrLinear = SnrDbToRatio (snrDb);

              double success = 0.0;

              // Find VHT mode index
              WifiMode mode (modeStr);
              WifiTxVector txVector;
              txVector.SetMode (mode);
              // VHT: NSS=1, 20MHz
              txVector.SetNss (1);
              txVector.SetChannelWidth (20);

              // For TableBasedErrorRateModel, need pre-generation
              if (emType == TABLE)
                {
                  static bool initialized = false;
                  if (!initialized)
                    {
                      // Generate the table for all modes
                      Ptr<TableBasedErrorRateModel> tableErm = erm->GetObject<TableBasedErrorRateModel> ();
                      tableErm->EnableLookupTableGeneration (true);
                      tableErm->UpdateLookupTables ();
                      initialized = true;
                    }
                }
              // Compute the Frame Success Rate
              double ber = erm->GetChunkSuccessRate (mode, txVector, snrLinear, frameSize * 8);

              // ber is probability of success for the frame
              success = ber;

              dataset.Add (snrDb, success);
            }
          datasets.push_back (dataset);
        }
      for (auto& d : datasets)
        {
          gnuPlot.AddDataset (d);
        }
      gnuPlot.SetTitle ("VHT Frame Success Rate vs SNR (" + modelName + ")");
      gnuPlot.SetLegend ("SNR (dB)", "Frame Success Rate");
      gnuPlot.SetTerminal ("png size 800,600");
      gnuPlot.SetExtra ("set yrange [0:1.05]");
      std::ofstream plotStream (plotFile, std::ios::out);
      gnuPlot.GenerateOutput (plotStream);
      plotStream.close ();
    }
  return 0;
}