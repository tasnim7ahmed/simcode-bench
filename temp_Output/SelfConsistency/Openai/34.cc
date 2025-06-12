/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Validate NIST, YANS, and Table-based error models for HE (High Efficiency) rates.
 * Plots Frame Success Rate vs SNR for each error rate model and all HE MCS values.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <string>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HeErrorModelValidation");

struct ModelConfig
{
  std::string label;
  Ptr<WifiPhy> phy;
  Ptr<ErrorRateModel> errModel;
};

static std::vector<WifiTxVector> GetHeTxVectors()
{
  std::vector<WifiTxVector> txVectors;
  // List all 12 HE MCS (0-11), single spatial stream, 20 MHz, SGI off, SU
  for (uint8_t mcs = 0; mcs <= 11; ++mcs)
    {
      WifiTxVector txVector;
      txVector.SetMode (WifiPhy::GetHeMcs0 () + mcs);
      txVector.SetNss (1);
      txVector.SetChannelWidth (20);
      txVector.SetGuardInterval (WifiPhy::GI_0_8us);
      txVector.SetPreambleType (WIFI_PREAMBLE_HE_SU);
      txVector.SetBssColor (0);
      txVector.SetCoding (0);
      txVectors.push_back (txVector);
    }
  return txVectors;
}

// Compute frame success rate for given ErrorRateModel, txvector, snr, bytes
static double CalcFrameSuccessRate(Ptr<ErrorRateModel> errModel, WifiTxVector txVector, double snrDb, uint32_t frameBytes)
{
  double snrLinear = std::pow (10.0, snrDb / 10.0);
  // Use the model to get a per-bit error probability
  double per = errModel->GetChunkSuccessRate (txVector, snrLinear, frameBytes * 8);
  return per;
}

int main(int argc, char* argv[])
{
  uint32_t frameBytes = 1500;

  CommandLine cmd;
  cmd.AddValue ("frameSize", "Frame size in bytes", frameBytes);
  cmd.Parse (argc, argv);

  // Prepare Gnuplot output
  Gnuplot gnuplot ("he-error-model-validation.plt");

  gnuplot.SetTitle ("Frame Success Rate vs SNR - HE MCS (802.11ax)");
  gnuplot.SetLegend ("SNR (dB)", "Frame Success Rate");
  gnuplot.AppendExtra("set log x");
  gnuplot.AppendExtra("set yrange [0:1]");
  gnuplot.AppendExtra("set style data linespoints");
  gnuplot.AppendExtra("set key outside");

  // Set up all the error rate models
  std::vector<ModelConfig> models;

  // For ErrorRateModel, we need a WifiPhy for context (interface type and band don't matter for model eval)
  // Use YansWifiPhy for context
  Ptr<YansWifiPhy> dummyPhy = CreateObject<YansWifiPhy> ();
  dummyPhy->SetChannel (CreateObject<YansWifiChannel> ());
  dummyPhy->SetErrorRateModel (CreateObject<YansErrorRateModel> ());

  // NIST
  Ptr<NistErrorRateModel> nistModel = CreateObject<NistErrorRateModel> ();
  models.push_back ({"NIST", dummyPhy, nistModel});

  // YANS
  Ptr<YansErrorRateModel> yansModel = CreateObject<YansErrorRateModel> ();
  models.push_back ({"YANS", dummyPhy, yansModel});

  // Table-based
  Ptr<TableBasedErrorRateModel> tableModel = CreateObject<TableBasedErrorRateModel> ();
  models.push_back ({"Table", dummyPhy, tableModel});

  // HE MCS vectors
  std::vector<WifiTxVector> heTxVectors = GetHeTxVectors();

  // X axis: SNR range
  std::vector<double> snrDbValues;
  for (double snr = 0.0; snr <= 50.0; snr += 1.0)
    {
      snrDbValues.push_back (snr);
    }

  // For each model and each MCS, generate a Gnuplot dataset
  for (const auto& model : models)
    {
      for (size_t mcsIdx = 0; mcsIdx < heTxVectors.size(); ++mcsIdx)
        {
          std::ostringstream dsLabel;
          dsLabel << model.label << " MCS" << mcsIdx;

          Gnuplot2dDataset dataset;
          dataset.SetTitle (dsLabel.str());
          dataset.SetStyle (Gnuplot2dDataset::LINES);

          for (const auto& snrDb : snrDbValues)
            {
              double fsr = CalcFrameSuccessRate (model.errModel, heTxVectors[mcsIdx], snrDb, frameBytes);
              dataset.Add (snrDb, fsr);
            }

          gnuplot.AddDataset (dataset);
        }
    }

  // Output to Gnuplot file
  std::ofstream plotFile ("he-error-model-validation.plt");
  gnuplot.GenerateOutput (plotFile);
  plotFile.close ();

  std::cout << "Gnuplot file 'he-error-model-validation.plt' generated." << std::endl;
  std::cout << "To plot, run: gnuplot -p he-error-model-validation.plt" << std::endl;

  Simulator::Destroy ();
  return 0;
}