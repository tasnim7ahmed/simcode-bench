#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/buildings-module.h"
#include "ns3/mmwave-helper.h"
#include "ns3/mmwave-3gpp-channel.h"
#include "ns3/mmwave-3gpp-propagation-loss-model.h"
#include "ns3/mmwave-antenna-array-model.h"
#include "ns3/df-fft-beamforming-algorithm.h"
#include "ns3/applications-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("V2X3gppSimulation");

std::ofstream g_logFile;
double g_snrSum = 0.0;
double g_pathlossSum = 0.0;
uint32_t g_samples = 0;

void
LogMetrics (Ptr<MmWave3gppPropagationLossModel> lossModel, Ptr<Node> txNode, Ptr<Node> rxNode, Ptr<SpectrumValue> rxPsd, Time now)
{
  // Compute distance, pathloss, and SNR
  Ptr<MobilityModel> txMob = txNode->GetObject<MobilityModel> ();
  Ptr<MobilityModel> rxMob = rxNode->GetObject<MobilityModel> ();
  double pathloss = lossModel->CalcRxPower (0.0, txMob, rxMob);

  // For SNR estimation
  double noiseFigure = 7.0; // dB, typical for mmWave/5G
  double noiseDbm = -174.0 + 10 * std::log10 (lossModel->GetFrequency () / 1e6) + noiseFigure;
  double snr = pathloss - noiseDbm;

  g_snrSum += snr;
  g_pathlossSum += pathloss;
  ++g_samples;

  g_logFile << now.GetSeconds () << "," << snr << "," << pathloss << std::endl;
}

int main (int argc, char *argv[])
{
  uint32_t nVehicles = 8;
  double simTime = 10.0;
  std::string scenario = "Urban";
  std::string logFileName = "v2x_3gpp_metrics.csv";
  uint32_t txAntennaH = 4, txAntennaV = 4;
  uint32_t rxAntennaH = 2, rxAntennaV = 2;
  double frequency = 28e9; // 28 GHz

  CommandLine cmd;
  cmd.AddValue ("nVehicles", "Number of vehicles (including one RSU/base station)", nVehicles);
  cmd.AddValue ("simTime", "Simulation time (s)", simTime);
  cmd.AddValue ("scenario", "Urban or Highway", scenario);
  cmd.Parse (argc, argv);

  // Log file
  g_logFile.open (logFileName.c_str (), std::ios::out);
  g_logFile << "Time,SNR,Pathloss" << std::endl;

  // Create RSU/Base Station and vehicles
  NodeContainer nodes;
  nodes.Create (nVehicles);

  // Mobility
  MobilityHelper mobility;

  if (scenario == "Urban")
    {
      Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator> ();
      posAlloc->Add (Vector (0.0, 0.0, 5.0)); // RSU at intersection, 5m height
      for (uint32_t i = 1; i < nVehicles; ++i)
        {
          posAlloc->Add (Vector (5*i, 10.0*i, 1.5)); // Vehicles at different locations
        }
      mobility.SetPositionAllocator (posAlloc);
      // Random direction 2D mobility for vehicles, RSU static
      mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
      mobility.Install (nodes.Get (0));
      mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue (Rectangle (-100, 100, -100, 100)),
                                "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=10.0]"),
                                "Distance", DoubleValue (40.0));
      for (uint32_t i = 1; i < nVehicles; ++i)
        mobility.Install (nodes.Get (i));
    }
  else // Highway
    {
      // RSU on the side, vehicles lined up, moving faster
      Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator> ();
      posAlloc->Add (Vector (0.0, 0.0, 5.0)); // RSU
      for (uint32_t i = 1; i < nVehicles; ++i)
        {
          posAlloc->Add (Vector (i*30, 0.0, 1.5)); // Vehicles along the x axis
        }
      mobility.SetPositionAllocator (posAlloc);
      mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
      mobility.Install (nodes.Get (0));
      mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
      for (uint32_t i = 1; i < nVehicles; ++i)
        {
          mobility.Install (nodes.Get (i));
          nodes.Get (i)->GetObject<ConstantVelocityMobilityModel> ()->SetVelocity (Vector (25.0, 0.0, 0.0));
        }
    }

  // 3GPP Channel and Loss
  Ptr<MmWave3gppPropagationLossModel> lossModel = CreateObject<MmWave3gppPropagationLossModel> ();
  if (scenario == "Urban")
    lossModel->SetAttribute ("Scenario", StringValue ("UMi-StreetCanyon"));
  else
    lossModel->SetAttribute ("Scenario", StringValue ("RMa"));
  lossModel->SetAttribute ("Frequency", DoubleValue (frequency));
  lossModel->SetAttribute ("ChannelCondition", StringValue ("a")); // LOS default

  // 3GPP Channel Model
  Ptr<MmWave3gppChannel> channel = CreateObject<MmWave3gppChannel> ();
  channel->SetPropagationLossModel (lossModel);
  channel->SetAttribute ("UpdatePeriod", TimeValue (MilliSeconds (100)));

  // Antenna arrays and beamforming by DFT
  std::vector<Ptr<MmWaveAntennaArrayModel>> txAntennaArr (nVehicles);
  std::vector<Ptr<MmWaveAntennaArrayModel>> rxAntennaArr (nVehicles);

  for (uint32_t i=0; i<nVehicles; ++i)
    {
      Ptr<MmWaveAntennaArrayModel> ant = CreateObject<MmWaveAntennaArrayModel> ();
      if (i==0)
        ant->SetAttribute ("NumRows", UintegerValue (txAntennaV));
      else
        ant->SetAttribute ("NumRows", UintegerValue (rxAntennaV));
      if (i==0)
        ant->SetAttribute ("NumColumns", UintegerValue (txAntennaH));
      else
        ant->SetAttribute ("NumColumns", UintegerValue (rxAntennaH));
      ant->SetAttribute ("AntennaElementSpacing", DoubleValue (0.5));
      if (i==0)
        txAntennaArr[i] = ant;
      else
        rxAntennaArr[i] = ant;
    }

  // DFT Beamforming
  Ptr<DfFftBeamformingAlgorithm> bfAlg = CreateObject<DfFftBeamformingAlgorithm> ();
  bfAlg->SetAttribute ("NumBeams", UintegerValue (txAntennaH*txAntennaV));
  // No explicit API to connect beamforming to channel here, but implied for logging

  // SpectrumValue for SNR computation, create a dummy for logging
  Ptr<SpectrumValue> rxPsd = Create<SpectrumValue> (1);

  // Set up event logging
  double logInterval = 0.1; // seconds

  for (double t=0.0; t<simTime; t+=logInterval)
    {
      for (uint32_t i=1; i<nVehicles; ++i)
        Simulator::Schedule (Seconds (t), &LogMetrics, lossModel, nodes.Get (0), nodes.Get (i), rxPsd, Seconds (t));
    }

  // Dummy network stack for completeness, no real traffic needed for SNR/pathloss
  InternetStackHelper internet;
  internet.Install (nodes);

  // Enable Buildings for proper 3GPP path loss
  BuildingsHelper::Install (nodes);

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  double avgSNR = (g_samples > 0) ? (g_snrSum/g_samples) : 0.0;
  double avgPL = (g_samples > 0) ? (g_pathlossSum/g_samples) : 0.0;

  std::cout << "Average SNR: " << avgSNR << " dB over " << g_samples << " samples" << std::endl;
  std::cout << "Average Pathloss: " << avgPL << " dB" << std::endl;

  g_logFile.close ();
  Simulator::Destroy ();
  return 0;
}