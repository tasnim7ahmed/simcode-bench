#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/buildings-module.h"
#include "ns3/wifi-module.h"
#include "ns3/antenna-module.h"
#include "ns3/propagation-module.h"
#include "ns3/channel-module.h"
#include "ns3/mmwave-module.h"
#include <fstream>
#include <vector>
#include <numeric>
#include <cmath>

using namespace ns3;

// Logging interval and output filename
static const double loggingInterval = 0.1; // seconds
static const std::string outputFileName = "vehicular_snr_pathloss.csv";

struct StatsCollector
{
  std::vector<double> snrs;
  std::vector<double> pathlosses;
  std::ofstream outFile;

  StatsCollector(const std::string &filename)
  {
    outFile.open(filename.c_str());
    outFile << "Time,AverageSNRdB,AveragePathlossdB\n";
  }

  ~StatsCollector()
  {
    outFile.close();
  }

  void Log(double time)
  {
    if (!snrs.empty() && !pathlosses.empty())
    {
      double avgSnr = std::accumulate(snrs.begin(), snrs.end(), 0.0) / snrs.size();
      double avgPl = std::accumulate(pathlosses.begin(), pathlosses.end(), 0.0) / pathlosses.size();
      outFile << time << "," << avgSnr << "," << avgPl << "\n";
      snrs.clear();
      pathlosses.clear();
    }
  }
};

Ptr<AntennaModel> CreateUPAAntenna(uint32_t numRows, uint32_t numCols)
{
  Ptr<UPAAntennaArrayModel> antenna = CreateObject<UPAAntennaArrayModel>();
  antenna->SetAttribute("NumRows", UintegerValue(numRows));
  antenna->SetAttribute("NumColumns", UintegerValue(numCols));
  antenna->SetAttribute("ElementSpacing", DoubleValue(0.5));
  return antenna;
}

void
BeamformingDFT(Ptr<NetDevice> txDev, Ptr<NetDevice> rxDev)
{
  Ptr<MmWaveNetDevice> txMmWave = DynamicCast<MmWaveNetDevice>(txDev);
  Ptr<MmWaveNetDevice> rxMmWave = DynamicCast<MmWaveNetDevice>(rxDev);

  if (txMmWave && rxMmWave)
  {
    Ptr<UPAAntennaArrayModel> txAnt = DynamicCast<UPAAntennaArrayModel>(txMmWave->GetAntenna());
    Ptr<UPAAntennaArrayModel> rxAnt = DynamicCast<UPAAntennaArrayModel>(rxMmWave->GetAntenna());

    if (txAnt && rxAnt)
    {
      // Use DFT codebook beamforming index 0 for both TX and RX
      txAnt->SetBeamformingVector(0); // Most straightforward beam
      rxAnt->SetBeamformingVector(0);
    }
  }
}

void
ComputeAndLogStats(
    Ptr<ChannelConditionModel> channelModel,
    NodeContainer vehicles,
    Ptr<StatsCollector> collector,
    double logTime,
    Ptr<NetDevice> bsDev,
    std::vector<Ptr<NetDevice>> ueDevs)
{
  for (uint32_t i = 0; i < vehicles.GetN(); ++i)
  {
    Ptr<Node> ue = vehicles.Get(i);
    Ptr<MobilityModel> mob = ue->GetObject<MobilityModel>();
    Ptr<MobilityModel> bsMob = bsDev->GetNode()->GetObject<MobilityModel>();

    // Pathloss in dB
    double pl = channelModel->GetLoss(bsMob, mob);
    // Assume txPower = 23dBm, NF=5dB, noiseFigure, bandwidth = 10MHz
    double bandwidth = 10e6;
    double noiseFigure = 5.0;
    double noisePwrDb = 10 * std::log10(1.38e-23 * 290 * bandwidth) + 30 + noiseFigure; // dBm
    double txPowerDbm = 23.0;

    double rxPowerDbm = txPowerDbm - pl;
    double snrDb = rxPowerDbm - noisePwrDb;

    collector->snrs.push_back(snrDb);
    collector->pathlosses.push_back(pl);
  }
  collector->Log(logTime);
  // Next logging event
  if (Simulator::Now().GetSeconds() + loggingInterval <= Simulator::GetDuration().GetSeconds())
  {
    Simulator::Schedule(Seconds(loggingInterval), &ComputeAndLogStats,
                        channelModel, vehicles, collector,
                        Simulator::Now().GetSeconds() + loggingInterval,
                        bsDev, ueDevs);
  }
}

int main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);

  // Scenario configuration
  uint32_t nVeh = 20;
  uint32_t nRowUe = 2, nColUe = 4;
  uint32_t nRowBs = 4, nColBs = 8;
  double simTime = 10.0; // seconds

  CommandLine cmd;
  cmd.AddValue("nVeh", "Number of vehicles", nVeh);
  cmd.AddValue("simTime", "Simulation time [s]", simTime);
  cmd.Parse(argc, argv);

  // Create nodes
  NodeContainer bs;
  bs.Create(1);
  NodeContainer vehicles;
  vehicles.Create(nVeh);

  // Mobility model: Base station fixed at center, vehicles initialized on a straight highway (urban+highway supported)
  // Urban: grid, Highway: random lane assignment with lane speeds
  Ptr<ListPositionAllocator> bsPos = CreateObject<ListPositionAllocator>();
  bsPos->Add(Vector(0.0, 0.0, 10.0)); // BS at origin, 10m above

  MobilityHelper bsMobHelper;
  bsMobHelper.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  bsMobHelper.SetPositionAllocator(bsPos);
  bsMobHelper.Install(bs);

  MobilityHelper vehMobHelper;
  // For urban, grid allocator, for highway, 3-lane straight
  Ptr<ListPositionAllocator> vehPos = CreateObject<ListPositionAllocator>();
  for (uint32_t i = 0; i < nVeh; ++i)
  {
    double lane = (i % 3) * 3.5; // 3.5m lane distance
    double posX = -100.0 + i * 15.0;
    vehPos->Add(Vector(posX, lane, 1.5));
  }
  vehMobHelper.SetPositionAllocator(vehPos);
  vehMobHelper.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  MobilityContainer vehMobility = vehMobHelper.Install(vehicles);

  for (uint32_t i = 0; i < nVeh; ++i)
  {
    Ptr<ConstantVelocityMobilityModel> cvmm = vehicles.Get(i)->GetObject<ConstantVelocityMobilityModel>();
    cvmm->SetVelocity(Vector(33.33, 0.0, 0.0)); // ~120 km/h along x-axis
  }

  // Attach buildings for urban propagation
  BuildingsHelper::Install(bs);
  BuildingsHelper::Install(vehicles);

  // Create channel - configure for 3GPP TR 38.901 Urban Microcell / Highway
  Ptr<ThreeGppChannelModel> channel = CreateObject<ThreeGppChannelModel>();
  channel->SetAttribute("Scenario", EnumValue(ThreeGppChannelModel::UMa));
  channel->SetAttribute("ChannelCondition", EnumValue(ThreeGppChannelCondition::LOS_NLOS));
  channel->SetAttribute("Frequency", DoubleValue(3.5e9));
  channel->SetAttribute("UpdatePeriod", TimeValue(MilliSeconds(100)));

  // NetDevices and Antenna assignment (UPAs)
  std::vector<Ptr<NetDevice>> ueDevs;
  Ptr<NetDevice> bsDev;

  MmWaveHelper mmwaveHelper;
  mmwaveHelper.SetChannelModel(channel);

  // BS device with antenna
  mmwaveHelper.SetAttribute("Antenna", PointerValue(CreateUPAAntenna(nRowBs, nColBs)));
  NodeContainer oneBs;
  oneBs.Add(bs.Get(0));
  NetDeviceContainer bsDevs = mmwaveHelper.InstallEnbDevice(oneBs);
  bsDev = bsDevs.Get(0);

  // UE devices with UPA antennas
  NetDeviceContainer ueNdDevs;
  for (uint32_t i = 0; i < nVeh; ++i)
  {
    mmwaveHelper.SetAttribute("Antenna", PointerValue(CreateUPAAntenna(nRowUe, nColUe)));
    NetDeviceContainer ueDev = mmwaveHelper.InstallUeDevice(NodeContainer(vehicles.Get(i)));
    ueDevs.push_back(ueDev.Get(0));
  }

  // Connect channel
  for (auto& ueDev : ueDevs)
  {
    BeamformingDFT(bsDev, ueDev);
  }

  // Channel model works directly on Mobility
  channel->SetMobility(bs.Get(0)->GetObject<MobilityModel>(), "eNB");
  for (uint32_t i = 0; i < nVeh; ++i)
  {
    channel->SetMobility(vehicles.Get(i)->GetObject<MobilityModel>(), "UE_" + std::to_string(i));
  }

  // Stats logger
  Ptr<StatsCollector> collector = CreateObject<StatsCollector>(outputFileName);

  Simulator::Schedule(Seconds(0.0), &ComputeAndLogStats, channel, vehicles, collector, 0.0, bsDev, ueDevs);

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}