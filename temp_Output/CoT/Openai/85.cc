#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/3gpp-channel-module.h"
#include "ns3/antenna-module.h"
#include "ns3/log.h"
#include <fstream>
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Vehicular3gppChannelExample");

class SnrPathlossTracer
{
public:
  SnrPathlossTracer (Ptr<ThreeGppChannelCondition> channelCond, Ptr<ThreeGppPropagationLossModel> propModel,
                     NodeContainer vehicles, Ptr<ThreeGppAntennaArrayModel> ueAntenna,
                     Ptr<ThreeGppAntennaArrayModel> bsAntenna, std::ofstream *out, double txPowerDbm, double noiseDbm)
    : m_channelCond (channelCond), m_propModel (propModel),
      m_vehicles (vehicles), m_ueAntenna (ueAntenna), m_bsAntenna (bsAntenna),
      m_out (out), m_txPowerDbm (txPowerDbm), m_noiseDbm (noiseDbm) {}

  void Trace (Time t)
  {
    double totalSnrDb = 0, totalPathloss = 0;
    uint32_t nPairs = 0;

    for (auto i = m_vehicles.Begin (); i != m_vehicles.End (); ++i)
      {
        Ptr<Node> ue = *i;
        Ptr<MobilityModel> ueMob = ue->GetObject<MobilityModel> ();
        Ptr<Node> bs = NodeList::GetNode (0); // assuming node 0 is BS
        Ptr<MobilityModel> bsMob = bs->GetObject<MobilityModel> ();

        ThreeGppChannelCondition::ChannelConditionValue condVal =
          m_channelCond->GetChannelCondition (bsMob, ueMob);
        double pathlossDb = m_propModel->CalcRxPower (0, ueMob, bsMob); // returns dBm

        // SNR[dB] = TxPower[dBm] - PathLoss[dB] - NoisePower[dBm]
        double snrDb = m_txPowerDbm - (-pathlossDb) - m_noiseDbm;

        // get approximate beamforming gain using DFT (assuming maximum, for demonstration)
        double bfGainDb = 10 * std::log10 (m_ueAntenna->GetNumElements () * m_bsAntenna->GetNumElements ());
        snrDb += bfGainDb; // apply beamforming gain

        totalSnrDb += snrDb;
        totalPathloss += (-pathlossDb);
        ++nPairs;
      }
    if (nPairs)
      {
        *m_out << t.GetSeconds () << " " << totalSnrDb / nPairs << " " << totalPathloss / nPairs << std::endl;
      }
    Simulator::Schedule (Seconds (0.1), &SnrPathlossTracer::Trace, this, Seconds (Simulator::Now ().GetSeconds () + 0.1));
  }

private:
  Ptr<ThreeGppChannelCondition> m_channelCond;
  Ptr<ThreeGppPropagationLossModel> m_propModel;
  NodeContainer m_vehicles;
  Ptr<ThreeGppAntennaArrayModel> m_ueAntenna;
  Ptr<ThreeGppAntennaArrayModel> m_bsAntenna;
  std::ofstream *m_out;
  double m_txPowerDbm;
  double m_noiseDbm;
};

int main (int argc, char *argv[])
{
  uint32_t numVehicles = 10;
  std::string scenario = "Urban"; // "Urban" or "Highway"
  double simTime = 10.0;
  double laneSpacing = 4.0;
  double highwayLength = 500.0;
  double urbanGridSize = 100.0;
  uint32_t bsAntennaRows = 4, bsAntennaCols = 8;
  uint32_t ueAntennaRows = 2, ueAntennaCols = 2; // UPA configuration
  double frequency = 3e9; // 3 GHz
  double txPowerDbm = 23.0;
  double noiseFgHz = 10e6;
  double noiseFigureDb = 7.0;
  double temp = 290.0;

  CommandLine cmd;
  cmd.AddValue ("numVehicles", "Number of vehicles", numVehicles);
  cmd.AddValue ("scenario", "Scenario: Urban or Highway", scenario);
  cmd.AddValue ("simTime", "Simulation duration (s)", simTime);
  cmd.Parse (argc, argv);

  // Create BS and vehicles
  NodeContainer bs;
  bs.Create (1);
  NodeContainer vehicles;
  vehicles.Create (numVehicles);

  // Mobility
  MobilityHelper bsMob, vehMob;
  if (scenario == "Urban")
    {
      bsMob.SetPositionAllocator ("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue (urbanGridSize / 2),
                                  "MinY", DoubleValue (urbanGridSize / 2),
                                  "DeltaX", DoubleValue (0),
                                  "DeltaY", DoubleValue (0),
                                  "GridWidth", UintegerValue (1),
                                  "LayoutType", StringValue ("RowFirst"));
      bsMob.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
      bsMob.Install (bs);

      // vehicles move on urban grid with random direction
      vehMob.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                   "X", StringValue ("ns3::UniformRandomVariable[Min=0|Max=100]"),
                                   "Y", StringValue ("ns3::UniformRandomVariable[Min=0|Max=100]"));
      vehMob.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                               "Mode", StringValue ("Time"),
                               "Time", TimeValue (Seconds (1.0)),
                               "Speed", StringValue ("ns3::UniformRandomVariable[Min=8|Max=15]"),
                               "Bounds", RectangleValue (Rectangle (0, urbanGridSize, 0, urbanGridSize)));
      vehMob.Install (vehicles);
    }
  else // Highway
    {
      bsMob.SetPositionAllocator ("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue (highwayLength / 2),
                                  "MinY", DoubleValue (laneSpacing),
                                  "DeltaX", DoubleValue (0),
                                  "DeltaY", DoubleValue (0),
                                  "GridWidth", UintegerValue (1),
                                  "LayoutType", StringValue ("RowFirst"));
      bsMob.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
      bsMob.Install (bs);

      vehMob.SetPositionAllocator ("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue (0),
                                  "MinY", DoubleValue (laneSpacing),
                                  "DeltaX", DoubleValue (highwayLength / (numVehicles + 1)),
                                  "DeltaY", DoubleValue (laneSpacing),
                                  "GridWidth", UintegerValue (numVehicles),
                                  "LayoutType", StringValue ("RowFirst"));
      vehMob.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
      vehMob.Install (vehicles);
      for (uint32_t i = 0; i < vehicles.GetN (); ++i)
        {
          Ptr<ConstantVelocityMobilityModel> mm = vehicles.Get (i)->GetObject<ConstantVelocityMobilityModel> ();
          mm->SetVelocity (Vector (25.0, 0.0, 0.0)); // 90 km/h
        }
    }

  // 3GPP channel and pathloss
  Ptr<ThreeGppChannelCondition> channelCond;
  Ptr<ThreeGppPropagationLossModel> propModel;

  if (scenario == "Urban")
    {
      ThreeGppChannelCondition::Parameters urbanParams;
      urbanParams.SetScenario (ThreeGppChannelCondition::Urban);
      channelCond = CreateObject<ThreeGppChannelCondition> (urbanParams);
      propModel = CreateObject<ThreeGppUrbanMicrocellPropagationLossModel> ();
    }
  else
    {
      ThreeGppChannelCondition::Parameters hwyParams;
      hwyParams.SetScenario (ThreeGppChannelCondition::Highway);
      channelCond = CreateObject<ThreeGppChannelCondition> (hwyParams);
      propModel = CreateObject<ThreeGppRuralMacrocellPropagationLossModel> ();
    }
  propModel->SetAttribute ("ChannelCondition", PointerValue (channelCond));
  propModel->SetAttribute ("Frequency", DoubleValue (frequency));
  propModel->SetAttribute ("ShadowingEnabled", BooleanValue (true)); // Enable shadowing
  propModel->SetAttribute ("UseShadowing", BooleanValue (true));

  // Antenna setup (UPA)
  Ptr<ThreeGppAntennaArrayModel> bsAntenna = Create<ThreeGppAntennaArrayModel> ();
  bsAntenna->SetRowsAndColumns (bsAntennaRows, bsAntennaCols);
  bsAntenna->SetElementSpacing (0.5 * 3e8 / frequency);
  bsAntenna->SetAntennaType (ThreeGppAntennaArrayModel::UPA);

  Ptr<ThreeGppAntennaArrayModel> ueAntenna = Create<ThreeGppAntennaArrayModel> ();
  ueAntenna->SetRowsAndColumns (ueAntennaRows, ueAntennaCols);
  ueAntenna->SetElementSpacing (0.5 * 3e8 / frequency);
  ueAntenna->SetAntennaType (ThreeGppAntennaArrayModel::UPA);

  // Add antennas to nodes
  bs.Get (0)->AggregateObject (bsAntenna);
  for (uint32_t i = 0; i < vehicles.GetN (); ++i)
    {
      vehicles.Get (i)->AggregateObject (ueAntenna);
    }

  // Install Internet stack (for demo, not used for performance evaluation)
  InternetStackHelper stack;
  stack.Install (bs);
  stack.Install (vehicles);

  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.255.255.0");
  NetDeviceContainer devices;
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Connect each vehicle to BS for demonstration
  for (uint32_t i = 0; i < vehicles.GetN (); ++i)
    {
      NetDeviceContainer link = p2p.Install (bs.Get (0), vehicles.Get (i));
      devices.Add (link);
      address.NewNetwork ();
      address.Assign (link);
    }

  // Noise power
  double k = 1.38064852e-23;
  double noisePowerW = k * temp * noiseFgHz;
  double noiseDbm = 10 * std::log10 (noisePowerW) + 30 + noiseFigureDb;

  // Logging SNR and pathloss to file
  std::ofstream outFile;
  outFile.open ("snr_pathloss.log", std::ios::out);
  outFile << "#time(s) avg_snr_db avg_pathloss_db\n";

  Ptr<SnrPathlossTracer> tracer = CreateObject<SnrPathlossTracer> (channelCond, propModel, vehicles,
                                                                   ueAntenna, bsAntenna, &outFile,
                                                                   txPowerDbm, noiseDbm);
  Simulator::Schedule (Seconds (0.0), &SnrPathlossTracer::Trace, tracer, Seconds (0.0));

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  outFile.close ();
  Simulator::Destroy ();

  return 0;
}