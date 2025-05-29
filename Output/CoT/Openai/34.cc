#include "ns3/core-module.h"
#include "ns3/gnuplot.h"
#include "ns3/wifi-module.h"
#include <vector>
#include <string>
#include <iomanip>

using namespace ns3;

static Ptr<YansWifiChannel> CreateDummyChannel ()
{
  Ptr<YansWifiChannel> channel = CreateObject<YansWifiChannel> ();
  Ptr<YansWifiPropagationLossModel> loss = CreateObject<ConstantSpeedPropagationDelayModel> ();
  channel->SetPropagationDelayModel (loss);
  return channel;
}

double CalculateFrameSuccessRate (Ptr<ErrorRateModel> model, WifiTxVector txVector, double snrDb, uint32_t frameSize)
{
  // Use a dummy header of frameSize bytes
  WifiMacHeader hdr;
  Ptr<Packet> pkt = Create<Packet> (frameSize);
  // SNR value conversion
  double snr = std::pow (10.0, snrDb / 10.0);

  // Table-based error model expects Table-based phy headers
  double per = model->GetChunkSuccessRate (pkt, txVector, snr, hdr);

  double fsr = 1.0 - per;
  if (fsr < 0.0)
    fsr = 0.0;
  if (fsr > 1.0)
    fsr = 1.0;
  return fsr;
}

int main (int argc, char *argv[])
{
  uint32_t frameSize = 1200;
  std::string outputPrefix = "he-fsr";
  double minSnr = 0, maxSnr = 40, snrStep = 1.0;
  CommandLine cmd;
  cmd.AddValue ("frameSize", "Size of data frame in bytes", frameSize);
  cmd.AddValue ("outputPrefix", "Prefix for output plot and data files", outputPrefix);
  cmd.AddValue ("minSnr", "Minimum SNR (in dB)", minSnr);
  cmd.AddValue ("maxSnr", "Maximum SNR (in dB)", maxSnr);
  cmd.AddValue ("snrStep", "SNR step size (in dB)", snrStep);
  cmd.Parse (argc, argv);

  std::vector<Ptr<ErrorRateModel>> models;
  std::vector<std::string> labels;
  models.push_back (CreateObject<NistErrorRateModel> ());
  labels.push_back ("NIST");
  models.push_back (CreateObject<YansErrorRateModel> ());
  labels.push_back ("YANS");
  models.push_back (CreateObject<TableBasedErrorRateModel> ());
  labels.push_back ("TableBased");

  Ptr<WifiPhy> phy = CreateObject<YansWifiPhy> ();
  phy->SetErrorRateModel (models[0]);
  phy->SetChannel (CreateDummyChannel ());

  WifiStandard standard = WIFI_STANDARD_80211ax;
  WifiMacHeader hdr;

  WifiPhyHelper phyHelper = WifiPhyHelper::Default ();
  WifiHelper wifiHelper;
  YansWifiPhyHelper yansPhy = YansWifiPhyHelper::Default ();
  wifiHelper.SetStandard (WIFI_STANDARD_80211ax);

  Ptr<WifiRemoteStationManager> manager = CreateObject<ConstantRateWifiManager> ();
  manager->SetAttribute ("DataMode", StringValue ("HeMcs0"));
  phy->SetErrorRateModel (models[0]);

  Ptr<HeWifiMac> heMac = CreateObject<HeWifiMac> ();
  heMac->ConfigureStandard (WIFI_STANDARD_80211ax);

  Ptr<WifiPhy> dummyPhy = phy;

  WifiTxVector txVector;
  txVector.SetPreambleType (WIFI_PREAMBLE_HE_SU);
  txVector.SetChannelWidth (20);
  txVector.SetGuardInterval (WIFI_GI_0_8_US);

  WifiModeFactory factory;
  std::vector<WifiMode> heModes;
  for (uint8_t mcs = 0; mcs <= 11; ++mcs)
    {
      WifiMode mode = WifiPhy::GetHeMcs (mcs, 0, 0, 20, false, false, false);
      heModes.push_back (mode);
    }

  uint32_t nModels = models.size ();
  uint32_t nModes = heModes.size ();

  std::vector<std::vector<std::vector<std::pair<double, double>>>> data (nModels, std::vector<std::vector<std::pair<double, double>>> (nModes));

  for (uint32_t modelIdx = 0; modelIdx < nModels; ++modelIdx)
    {
      phy->SetErrorRateModel (models[modelIdx]);
      for (uint32_t modeIdx = 0; modeIdx < nModes; ++modeIdx)
        {
          txVector.SetMode (heModes[modeIdx]);
          for (double snrDb = minSnr; snrDb <= maxSnr; snrDb += snrStep)
            {
              double fsr = CalculateFrameSuccessRate (models[modelIdx], txVector, snrDb, frameSize);
              data[modelIdx][modeIdx].push_back (std::make_pair (snrDb, fsr));
            }
        }
    }

  // Output Gnuplot datasets and scripts
  std::vector<std::string> mcsLabels;
  for (uint32_t modeIdx = 0; modeIdx < nModes; ++modeIdx)
    {
      std::ostringstream oss;
      oss << "MCS " << modeIdx;
      mcsLabels.push_back (oss.str ());
    }

  for (uint32_t modelIdx = 0; modelIdx < nModels; ++modelIdx)
    {
      for (uint32_t modeIdx = 0; modeIdx < nModes; ++modeIdx)
        {
          std::ostringstream fn;
          fn << outputPrefix << "-model" << modelIdx << "-mcs" << modeIdx << ".dat";
          std::ofstream f (fn.str ());
          for (auto &dp : data[modelIdx][modeIdx])
            {
              f << std::setprecision (5) << dp.first << "\t" << dp.second << std::endl;
            }
          f.close ();
        }
      std::ostringstream script;
      script << outputPrefix << "-" << labels[modelIdx] << ".plt";
      std::ofstream scriptFile (script.str ());
      scriptFile << "set title 'Frame Success Rate vs SNR (" << labels[modelIdx] << " Error Rate Model)'\n";
      scriptFile << "set xlabel 'SNR (dB)'\n";
      scriptFile << "set ylabel 'Frame Success Rate'\n";
      scriptFile << "set grid\n";
      scriptFile << "set key left top\n";
      scriptFile << "plot ";
      for (uint32_t modeIdx = 0; modeIdx < nModes; ++modeIdx)
        {
          if (modeIdx > 0)
            scriptFile << ", ";
          scriptFile << "'" << outputPrefix << "-model" << modelIdx << "-mcs" << modeIdx << ".dat' "
                     << "with lines title '" << mcsLabels[modeIdx] << "'";
        }
      scriptFile << std::endl;
      scriptFile.close ();
    }

  std::cout << "Generated Gnuplot .dat and .plt files for each model and HE MCS.\n";
  return 0;
}