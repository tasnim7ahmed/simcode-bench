#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HtErrorRateValidation");

class HtErrorRateValidation
{
public:
  HtErrorRateValidation();
  void Run(uint32_t frameSize);

private:
  void SetupSimulation(double snr, WifiStandard standard, WifiPreamble preamble, uint32_t frameSize);
  double CalculateFrameSuccessRate(Ptr<WifiPhy> phy, Ptr<Packet> packet, WifiTxVector txVector, double snr);
  Gnuplot2dDataset GeneratePlotDataset(const std::string& name, const std::vector<double>& snrValues, const std::vector<std::vector<double>>& fsrResults);

  std::vector<WifiMode> m_htModes;
};

HtErrorRateValidation::HtErrorRateValidation()
{
  // HT MCS modes from 0 to 31 (HT MCS index range)
  for (uint8_t mcs = 0; mcs <= 31; ++mcs)
    {
      m_htModes.push_back(WifiModeFactory::Get().CreateWifiMode("HTMCS" + std::to_string(mcs)));
    }
}

void HtErrorRateValidation::Run(uint32_t frameSize)
{
  std::vector<double> snrValues;
  for (double snr = -5.0; snr <= 30.0; snr += 2.0)
    {
      snrValues.push_back(snr);
    }

  uint32_t numMcs = m_htModes.size();
  std::vector<std::vector<double>> nistFsr(numMcs, std::vector<double>(snrValues.size()));
  std::vector<std::vector<double>> yansFsr(numMcs, std::vector<double>(snrValues.size()));
  std::vector<std::vector<double>> tableFsr(numMcs, std::vector<double>(snrValues.size()));

  // Create a packet with the given size
  Ptr<Packet> packet = Create<Packet>(frameSize);

  // HT configuration
  WifiTxVector txVector;
  txVector.SetMode(WifiMode("HTMCS0"));
  txVector.SetNss(1); // One spatial stream

  for (size_t i = 0; i < snrValues.size(); ++i)
    {
      double snr = snrValues[i];
      NS_LOG_DEBUG("Processing SNR: " << snr);

      // NIST error rate model
      Config::SetDefault("ns3::WifiRemoteStationManager::ErrorRateModel", StringValue("ns3::NistErrorRateModel"));
      for (size_t j = 0; j < numMcs; ++j)
        {
          txVector.SetMode(m_htModes[j]);
          nistFsr[j][i] = CalculateFrameSuccessRate(nullptr, packet, txVector, snr);
        }

      // YANS error rate model
      Config::SetDefault("ns3::WifiRemoteStationManager::ErrorRateModel", StringValue("ns3::YansErrorRateModel"));
      for (size_t j = 0; j < numMcs; ++j)
        {
          txVector.SetMode(m_htModes[j]);
          yansFsr[j][i] = CalculateFrameSuccessRate(nullptr, packet, txVector, snr);
        }

      // Table-based error rate model
      Config::SetDefault("ns3::WifiRemoteStationManager::ErrorRateModel", StringValue("ns3::TableBasedErrorRateModel"));
      for (size_t j = 0; j < numMcs; ++j)
        {
          txVector.SetMode(m_htModes[j]);
          tableFsr[j][i] = CalculateFrameSuccessRate(nullptr, packet, txVector, snr);
        }
    }

  // Output Gnuplot datasets
  Gnuplot gnuplot;
  gnuplot.SetTitle("Frame Success Rate vs SNR for HT MCS Indexes");
  gnuplot.SetTerminal("png");
  gnuplot.SetLegend("SNR (dB)", "Frame Success Rate");

  gnuplot.AddDataset(GeneratePlotDataset("NIST", snrValues, nistFsr));
  gnuplot.AddDataset(GeneratePlotDataset("YANS", snrValues, yansFsr));
  gnuplot.AddDataset(GeneratePlotDataset("Table-Based", snrValues, tableFsr));

  std::ofstream plotFile("ht-error-rate-validation.plt");
  gnuplot.GenerateOutput(plotFile);
}

void HtErrorRateValidation::SetupSimulation(double snr, WifiStandard standard, WifiPreamble preamble, uint32_t frameSize)
{
  // Not used here, but could be extended for future use
}

double HtErrorRateValidation::CalculateFrameSuccessRate(Ptr<WifiPhy> phy, Ptr<Packet> packet, WifiTxVector txVector, double snr)
{
  if (!phy)
    {
      // Create minimal PHY and channel setup for error rate calculation
      NodeContainer nodes;
      nodes.Create(2);

      InternetStackHelper stack;
      stack.Install(nodes);

      WifiHelper wifi;
      wifi.SetStandard(WIFI_STANDARD_80211n_5MHZ);
      wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", WifiModeValue(txVector.GetMode()), "ControlMode", WifiModeValue(txVector.GetMode()));

      YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
      channel.AddPropagationLoss("ns3::LogDistancePropagationLossModel");
      YansWifiPhyHelper phyHelper;
      phyHelper.SetChannel(channel.Create());
      phyHelper.Set("TxPowerStart", DoubleValue(16));
      phyHelper.Set("TxPowerEnd", DoubleValue(16));
      phyHelper.Set("RxGain", DoubleValue(0));
      phyHelper.Set("CcaEdThreshold", DoubleValue(-95)); // dBm

      WifiMacHelper mac;
      Ssid ssid = Ssid("ht-test");
      mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

      NetDeviceContainer staDevices;
      staDevices = wifi.Install(phyHelper, mac, nodes.Get(0));

      mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
      NetDeviceContainer apDevices;
      apDevices = wifi.Install(phyHelper, mac, nodes.Get(1));

      phy = DynamicCast<WifiPhy>(phyHelper.GetPhy(staDevices.Get(0)));
    }

  // Simulate reception using PhyReceiver
  Ptr<Packet> copy = packet->Copy();
  WifiConstPsduMap psduMap;
  WifiTxVector txVectorFinal = txVector;
  Time duration = phy->CalculateTxDuration(copy->GetSize(), txVectorFinal, WIFI_PREAMBLE_LONG, phy->GetFrequency(), 0, 0);
  Ptr<WifiPpdu> ppdu = Create<WifiPpdu>(psduMap, txVectorFinal, duration, WIFI_PREAMBLE_LONG, phy->GetFrequency());
  Ptr<SpectrumValue> txPowerSpectrum = WifiSpectrumValueHelper::CreateHeTxPowerSpectralDensity(phy->GetFrequency(), 20, 16, 0);
  Ptr<SpectrumValue> rxPowerSpectrum = Copy<SpectrumValue>(txPowerSpectrum);
  (*rxPowerSpectrum) *= std::pow(10.0, snr / 10.0); // Apply SNR

  Ptr<InterferenceHelper> interference = CreateObject<InterferenceHelper>();
  interference->AddSignal(rxPowerSpectrum, NanoSeconds(0));
  interference->EraseEvents();

  PhyEntity::SignalNoiseDbm signalNoise;
  signalNoise.signal = 10 * std::log10(interference->GetSignalPower()) + 30; // Convert W to dBm
  signalNoise.noise = 10 * std::log10(interference->GetNoisePower()) + 30;

  double totalBitCount = copy->GetSize() * 8;
  double errorProbability = phy->CalculatePe(ppdu, signalNoise.signal - signalNoise.noise);
  double successRate = (errorProbability >= 0 && errorProbability <= 1) ? (1 - errorProbability) : 0;

  return successRate;
}

Gnuplot2dDataset HtErrorRateValidation::GeneratePlotDataset(const std::string& name, const std::vector<double>& snrValues, const std::vector<std::vector<double>>& fsrResults)
{
  Gnuplot2dDataset dataset;
  dataset.SetTitle(name);
  dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

  for (size_t i = 0; i < fsrResults.size(); ++i)
    {
      for (size_t j = 0; j < snrValues.size(); ++j)
        {
          dataset.Add(snrValues[j], fsrResults[i][j]);
        }
      // Add blank line between MCS curves
      dataset.AddEmptyLine();
    }

  return dataset;
}

int main(int argc, char* argv[])
{
  uint32_t frameSize = 1472; // Default frame size in bytes

  CommandLine cmd(__FILE__);
  cmd.AddValue("frameSize", "The size of the frame to transmit (bytes)", frameSize);
  cmd.Parse(argc, argv);

  HtErrorRateValidation simulation;
  simulation.Run(frameSize);

  return 0;
}