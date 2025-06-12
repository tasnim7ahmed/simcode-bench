#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <string>
#include <sstream>

using namespace ns3;

struct ErrorModelTest
{
  std::string name;
  Ptr<YansWifiPhy> phy;
  Ptr<ErrorRateModel> errModel;
};

static std::string GetMcsString(uint8_t mcs)
{
  std::ostringstream oss;
  oss << "HE-MCS" << unsigned(mcs);
  return oss.str();
}

int main(int argc, char* argv[])
{
  uint32_t frameSize = 1500;
  double snrMin = 0;
  double snrMax = 40;
  double snrStep = 1;
  std::string outputPrefix = "he-error-models";
  CommandLine cmd;
  cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
  cmd.AddValue("snrMin",    "Minimum SNR (dB)", snrMin);
  cmd.AddValue("snrMax",    "Maximum SNR (dB)", snrMax);
  cmd.AddValue("snrStep",   "SNR step (dB)",    snrStep);
  cmd.AddValue("outputPrefix", "Output file prefix", outputPrefix);
  cmd.Parse(argc, argv);

  WifiPhyStandard standard = WIFI_PHY_STANDARD_80211ax_5GHZ;

  // Use YansWifiPhy as PHY object just to hold ErrorRateModels (not for simulating propagation).
  Ptr<YansWifiPhy> dummyPhy = CreateObject<YansWifiPhy>();
  Ptr<HeWifiMac> dummyMac = CreateObject<HeWifiMac>();
  dummyPhy->SetChannel(CreateObject<YansWifiChannel>());

  // Set up ErrorRateModels
  std::vector<ErrorModelTest> models;

  {
    Ptr<NistErrorRateModel> nist = CreateObject<NistErrorRateModel>();
    dummyPhy->SetErrorRateModel(nist);
    models.push_back({"NIST", dummyPhy, nist});
  }
  {
    Ptr<YansErrorRateModel> yans = CreateObject<YansErrorRateModel>();
    dummyPhy->SetErrorRateModel(yans);
    models.push_back({"YANS", dummyPhy, yans});
  }
  {
    Ptr<TableBasedErrorRateModel> table = CreateObject<TableBasedErrorRateModel>();
    // Init HE default tables
    table->SetHeTable();
    dummyPhy->SetErrorRateModel(table);
    models.push_back({"TableBased", dummyPhy, table});
  }

  const uint8_t nHeMcs = 12; // 0..11 for single spatial stream HE80

  for (auto& model : models)
    {
      Gnuplot plot;
      plot.SetTitle("Frame Success Rate vs SNR (" + model.name + " Error Model)");
      plot.SetLegend("SNR [dB]", "Frame Success Rate");
      std::vector<Gnuplot2dDataset> datasets(nHeMcs);

      // Set legend for each dataset (one per MCS)
      for (uint8_t mcs = 0; mcs < nHeMcs; ++mcs)
        {
          datasets[mcs].SetTitle(GetMcsString(mcs));
          datasets[mcs].SetStyle(Gnuplot2dDataset::LINES);
        }

      for (uint8_t mcs = 0; mcs < nHeMcs; ++mcs)
        {
          WifiMode mode = WifiPhy::GetHeMcs(mcs, 1, HE_CHANNEL_WIDTH_80);
          Ptr<WifiPhy> phy = model.phy;
          for (double snr = snrMin; snr <= snrMax; snr += snrStep)
            {
              double noise = 1e-9; // arbitrary, cancels in ratio
              double signal = noise * std::pow(10.0, snr / 10.0);

              double ber = model.errModel->GetChunkSuccessRate(mode, static_cast<uint32_t>(frameSize * 8), signal, noise);
              datasets[mcs].Add(snr, ber);
            }
        }

      for (uint8_t mcs = 0; mcs < nHeMcs; ++mcs)
        plot.AddDataset(datasets[mcs]);

      std::string fileName = outputPrefix + "-" + model.name + ".plt";
      std::ofstream plotFile(fileName);
      plot.GenerateOutput(plotFile);
      plotFile.close();
    }

  return 0;
}