#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/buildings-module.h"
#include "ns3/antenna-module.h"
#include "ns3/config-store-module.h"
#include "ns3/propagation-module.h"
#include "ns3/log.h"
#include <fstream>
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("V2XBeamformingSim");

class SNRLogger : public Object
{
public:
  SNRLogger () : m_count (0), m_sumSNR (0), m_sumPL (0) {}
  void Log (double snrDb, double plDb)
  {
    m_count++;
    m_sumSNR += snrDb;
    m_sumPL += plDb;
  }
  double GetAvgSNR () const { return (m_count > 0 ? m_sumSNR / m_count : 0); }
  double GetAvgPL () const { return (m_count > 0 ? m_sumPL / m_count : 0); }
  void Reset () { m_count = 0; m_sumSNR = 0; m_sumPL = 0; }
private:
  uint32_t m_count;
  double m_sumSNR;
  double m_sumPL;
};

void LogMetrics (Ptr<SNRLogger> logger, Ptr<Node> tx, Ptr<Node> rx, Ptr<ThreeGppChannelModel> channel, std::ofstream &outfile, double txPowerDbm, uint32_t nTx, uint32_t nRx)
{
  Ptr<MobilityModel> txMob = tx->GetObject<MobilityModel>();
  Ptr<MobilityModel> rxMob = rx->GetObject<MobilityModel>();
  double lambda = 3e8 / 2e9; // 2 GHz

  Ptr<AntennaModel> txAnt = tx->GetDevice(0)->GetObject<AntennaModel>();
  Ptr<AntennaModel> rxAnt = rx->GetDevice(0)->GetObject<AntennaModel>();

  NS_ASSERT (channel != 0);
  double pathloss = channel->GetLossDb (txMob, rxMob);
  // Beamforming vector (simplified DFT, selecting main beam to receiver)
  Vector txPos = txMob->GetPosition ();
  Vector rxPos = rxMob->GetPosition ();
  double dx = rxPos.x - txPos.x;
  double dy = rxPos.y - txPos.y;
  double az = std::atan2 (dy, dx);

  Ptr<UniformPlanarArrayAntennaModel> txUpa = DynamicCast<UniformPlanarArrayAntennaModel> (txAnt);
  Ptr<UniformPlanarArrayAntennaModel> rxUpa = DynamicCast<UniformPlanarArrayAntennaModel> (rxAnt);

  double txGain = 0;
  double rxGain = 0;
  if (txUpa)
    txGain = txUpa->GetGainDb (az, 0);
  else
    txGain = txAnt->GetGainDb (az, 0);

  if (rxUpa)
    rxGain = rxUpa->GetGainDb (az + M_PI, 0);
  else
    rxGain = rxAnt->GetGainDb (az + M_PI, 0);

  double txPowerW = std::pow (10.0, txPowerDbm/10.0) / 1000;
  double pathlossLinear = std::pow (10.0, -pathloss/10.0);
  double rxPowerW = txPowerW * std::pow(10, (txGain + rxGain)/10.0) * pathlossLinear;
  double N0 = 1.38e-23 * 300 * 9e6; // k*T*B, B=9MHz, 300K
  double snr = rxPowerW / N0;
  double snrDb = 10.0 * std::log10 (snr);

  logger->Log (snrDb, pathloss);
  outfile << Simulator::Now ().GetSeconds () << "," << snrDb << "," << pathloss << "\n";
}

void PeriodicLog (Ptr<SNRLogger> logger, Ptr<Node> tx, Ptr<Node> rx, Ptr<ThreeGppChannelModel> channel, std::ofstream *outfile, 
                  double txPowerDbm, uint32_t nTx, uint32_t nRx, double interval)
{
  LogMetrics (logger, tx, rx, channel, *outfile, txPowerDbm, nTx, nRx);

  Simulator::Schedule (Seconds (interval), &PeriodicLog, logger, tx, rx, channel, outfile, 
                       txPowerDbm, nTx, nRx, interval);
}

int main (int argc, char *argv[])
{
  uint32_t numVehicles = 6;
  double simTime = 10.0;
  double logInterval = 0.1;
  std::string scenario = "urban";

  CommandLine cmd;
  cmd.AddValue ("scenario", "urban or highway", scenario);
  cmd.AddValue ("simTime", "Simulation time (s)", simTime);
  cmd.AddValue ("numVehicles", "Number of vehicles", numVehicles);
  cmd.Parse (argc, argv);

  NodeContainer vehicles;
  vehicles.Create (numVehicles);

  MobilityHelper mobility;
  if (scenario == "urban")
    {
      mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue (0.0),
                                     "MinY", DoubleValue (0.0),
                                     "DeltaX", DoubleValue (20.0),
                                     "DeltaY", DoubleValue (10.0),
                                     "GridWidth", UintegerValue (3),
                                     "LayoutType", StringValue ("RowFirst"));

      mobility.SetMobilityModel ("ns3::RandomDirection2dMobilityModel",
                                 "Bounds", RectangleValue (Rectangle (-50, 150, -50, 100)),
                                 "Speed", StringValue ("ns3::UniformRandomVariable[Min=5.0|Max=15.0]"),
                                 "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"));
    }
  else // highway
    {
      mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue (0.0),
                                     "MinY", DoubleValue (0.0),
                                     "DeltaX", DoubleValue (30.0),
                                     "DeltaY", DoubleValue (4.0),
                                     "GridWidth", UintegerValue (numVehicles),
                                     "LayoutType", StringValue ("RowFirst"));

      mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
    }

  mobility.Install (vehicles);

  if (scenario == "highway")
    {
      for (uint32_t i = 0; i < vehicles.GetN (); ++i)
        {
          Ptr<ConstantVelocityMobilityModel> mob = vehicles.Get (i)->GetObject<ConstantVelocityMobilityModel> ();
          mob->SetVelocity (Vector (30.0, 0, 0)); // 30m/s along x
        }
    }

  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      BuildingsHelper::Install (vehicles.Get (i));
    }

  uint32_t nTx = 8, nRx = 8, txAntsX = 4, txAntsY = 2, rxAntsX = 4, rxAntsY = 2;
  double freq = 2e9;
  double lambda = 3e8 / freq;
  double elementSpacing = lambda / 2;

  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      Ptr<UniformPlanarArrayAntennaModel> txAnt = CreateObject<UniformPlanarArrayAntennaModel> ();
      txAnt->SetAttribute ("MaxGain", DoubleValue (8.0));
      txAnt->SetAttribute ("NumColumns", UintegerValue (txAntsX));
      txAnt->SetAttribute ("NumRows", UintegerValue (txAntsY));
      txAnt->SetAttribute ("ColumnSpacing", DoubleValue (elementSpacing));
      txAnt->SetAttribute ("RowSpacing", DoubleValue (elementSpacing));
      vehicles.Get (i)->AggregateObject (txAnt);
    }

  // Pathloss and Channel Model Setup (3GPP compliant)
  Ptr<ThreeGppPropagationLossModel> plModel;
  if (scenario == "urban")
    {
      plModel = CreateObject<BuildingsPropagationLossModelThreeGppUrbanMicro> ();
      plModel->SetAttribute ("Frequency", DoubleValue (freq));
    }
  else // highway
    {
      plModel = CreateObject<ThreeGppHighwayPropagationLossModel> ();
      plModel->SetAttribute ("Frequency", DoubleValue (freq));
    }

  Ptr<ThreeGppChannelModel> channel = CreateObject<ThreeGppChannelModel> ();
  channel->SetAttribute ("PropagationLossModel", PointerValue (plModel));
  channel->SetAttribute ("Frequency", DoubleValue (freq));

  // Assign antennas to all nodes
  ObjectMap movAntenna;
  for (uint32_t i = 0; i < numVehicles; ++i)
    movAntenna[vehicles.Get (i)] = vehicles.Get (i)->GetObject<UniformPlanarArrayAntennaModel> ();
  channel->SetAttribute ("AntennaModelMap", ObjectMapValue (movAntenna));

  // Logging setup
  Ptr<SNRLogger> logger = CreateObject<SNRLogger> ();
  std::ofstream outfile ("v2x_metrics.csv");
  outfile << "time,snr_db,pathloss_db\n";

  double txPowerDbm = 23.0;

  // For demonstration, log between first and second vehicle
  Simulator::ScheduleNow (&PeriodicLog, logger, vehicles.Get (0), vehicles.Get (1), channel, &outfile,
                          txPowerDbm, nTx, nRx, logInterval);

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();
  outfile.close ();

  // Terminal summary
  std::ofstream summary ("v2x_metrics_summary.txt");
  summary << "Average SNR (dB): " << logger->GetAvgSNR () << "\n";
  summary << "Average Pathloss (dB): " << logger->GetAvgPL () << "\n";
  summary.close ();

  return 0;
}