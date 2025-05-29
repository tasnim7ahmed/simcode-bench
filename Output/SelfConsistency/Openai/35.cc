/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <string>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("OfdmErrorRateModelsValidation");

struct OfdmMode
{
  WifiMode mode;
  std::string label;
};

std::vector<OfdmMode>
GetOfdmModes ()
{
  WifiPhyStandard standard = WIFI_PHY_STANDARD_80211a;
  WifiHelper wifi;
  wifi.SetStandard (standard);
  Ptr<WifiPhy> phy = CreateObject<YansWifiPhy> ();
  phy->ConfigureStandard (standard);
  WifiMacHelper mac;
  NetDeviceContainer devices = wifi.Install (phy, mac, NodeContainer ());
  std::vector<OfdmMode> ofdmModes;
  uint32_t numModes = WifiPhy::GetNBssRates (WIFI_PHY_STANDARD_80211a);

  for (uint32_t i = 0; i < phy->GetNModes (); ++i)
    {
      WifiMode mode = phy->GetMode (i);
      if (mode.GetModulationClass () == WIFI_MOD_CLASS_OFDM &&
         mode.GetBandwidth () == 20 && // Legacy 20 MHz OFDM
         mode.GetDataRate () > 0)
        {
          std::ostringstream oss;
          oss << mode.GetDataRate () / 1e6 << " Mbps";
          ofdmModes.push_back ({mode, oss.str ()});
        }
    }
  return ofdmModes;
}

double
DbToRatio (double db)
{
  return std::pow (10.0, db / 10.0);
}

struct ErrorModelSet
{
  std::string name;
  Ptr<ErrorRateModel> model;
};

int
main (int argc, char *argv[])
{
  uint32_t frameSize = 1200;
  CommandLine cmd;
  cmd.AddValue ("frameSize", "Frame size in bytes", frameSize);
  cmd.Parse (argc, argv);

  std::vector<ErrorModelSet> errorModels;

  errorModels.push_back (
    {"Nist", CreateObject<NistErrorRateModel> ()});
  errorModels.push_back (
    {"Yans", CreateObject<YansErrorRateModel> ()});
  errorModels.push_back (
    {"Table", CreateObject<TableBasedErrorRateModel> ()});

  std::vector<OfdmMode> ofdmModes = GetOfdmModes ();

  // SNR Range (dB)
  double snrStart = -5.0;
  double snrEnd = 30.0;
  double snrStep = 1.0;

  for (const auto& errorModel : errorModels)
    {
      Gnuplot plot (errorModel.name + "-ofdm-snr-fsr.png");
      plot.SetTerminal ("png size 900,600");
      plot.SetTitle ("OFDM Frame Success Rate vs SNR (" + errorModel.name + " Error Rate Model)");
      plot.SetLegend ("SNR (dB)", "Frame Success Rate");

      for (const auto& ofdmMode : ofdmModes)
        {
          Gnuplot2dDataset dataset (ofdmMode.label);
          dataset.SetStyle (Gnuplot2dDataset::LINES);

          for (double snrDb = snrStart; snrDb <= snrEnd + 1e-6; snrDb += snrStep)
            {
              double snr = DbToRatio (snrDb);

              double per = errorModel.model->GetChunkSuccessRate (
                  ofdmMode.mode,
                  snr,
                  frameSize * 8 // bits
                  );
              double fsr = per;

              dataset.Add (snrDb, fsr);
            }
          plot.AddDataset (dataset);
        }
      std::ofstream plotFile (errorModel.name + "-ofdm-snr-fsr.plt");
      plot.GenerateOutput (plotFile);
      plotFile.close ();
    }

  Simulator::Destroy ();
  return 0;
}