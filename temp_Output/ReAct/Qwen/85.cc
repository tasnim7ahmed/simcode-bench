#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/mmwave-module.h"
#include "ns3/config-store-module.h"
#include "ns3/antenna-module.h"
#include "ns3/applications-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VehicularBeamformingSimulation");

class SimulationHelper {
public:
  static Ptr<UniformPlanarArray> CreateAntennaArray(uint32_t numElementsX, uint32_t numElementsY) {
    Ptr<UniformPlanarArray> antenna = CreateObject<UniformPlanarArray>();
    antenna->SetNumColumns(numElementsX);
    antenna->SetNumRows(numElementsY);
    return antenna;
  }

  static void LogPathlossAndSNR(Ptr<const SpectrumPhy> txPhy, Ptr<const SpectrumPhy> rxPhy,
                                const SpectrumValue& sinr, const SpectrumValue& power) {
    double avgSinr = Avg(sinr);
    double avgPower = Avg(power);
    double pathlossDb = 10 * log10(power / (txPhy->GetTxPowerLevel() * 1e-3));
    std::ofstream outFile("pathloss_snr_log.csv", std::ios_base::app);
    outFile << Simulator::Now().GetSeconds() << ","
            << avgSinr << ","
            << pathlossDb << std::endl;
    outFile.close();
  }
};

int main(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  // Setup logging
  LogComponentEnable("VehicularBeamformingSimulation", LOG_LEVEL_INFO);

  // Create nodes for vehicles
  NodeContainer nodes;
  nodes.Create(2); // Two vehicle nodes

  // Mobility setup: urban and highway scenarios
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(nodes);

  // Set positions and velocities
  Ptr<MobilityModel> mob0 = nodes.Get(0)->GetObject<MobilityModel>();
  mob0->SetPosition(Vector(0.0, 0.0, 1.5)); // Urban vehicle at origin
  nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(20.0, 0.0, 0.0));

  Ptr<MobilityModel> mob1 = nodes.Get(1)->GetObject<MobilityModel>();
  mob1->SetPosition(Vector(100.0, 0.0, 1.5)); // Highway vehicle
  nodes.Get(1)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(35.0, 0.0, 0.0));

  // Setup propagation channel using 3GPP TR 37.885 and TR 38.901
  Config::SetDefault("ns3::MmWaveHelper::PathlossModel", StringValue("ns3::ThreeGppUmiStreetCanyonPropagationLossModel"));
  Config::SetDefault("ns3::MmWaveComponentCarrier::Scenario", StringValue("UMa"));
  Config::SetDefault("ns3::MmWaveHelper::ChannelCondition", StringValue("ns3::ThreeGppChannelConditionModel"));

  // Antennas: Uniform Planar Arrays
  uint32_t numTxElementsX = 16;
  uint32_t numTxElementsY = 1;
  uint32_t numRxElementsX = 16;
  uint32_t numRxElementsY = 1;

  // Beamforming with DFT method
  Config::SetDefault("ns3::MmWaveBeamformingModel::BeamformingMethod", StringValue("DirectFourierTransform"));

  // Install MmWave devices
  MmWaveHelper mmWaveHelper;
  mmWaveHelper.Initialize();

  NetDeviceContainer devices = mmWaveHelper.Install(nodes);
  mmWaveHelper.AttachToClosestEnb(devices, devices);

  // Assign antennas to devices
  for (uint32_t i = 0; i < devices.GetN(); ++i) {
    Ptr<NetDevice> device = devices.Get(i);
    Ptr<MmWaveNetDevice> mmWaveDevice = DynamicCast<MmWaveNetDevice>(device);
    if (mmWaveDevice) {
      Ptr<AntennaModel> txAntenna = CreateObject<AntennaModel>();
      txAntenna->SetAttribute("SpectrumModel", PointerValue(SpectrumModel()));
      txAntenna->SetAttribute("AntennaElementPattern", StringValue("ns3::IsotropicAntennaElement"));
      mmWaveDevice->SetAntenna(txAntenna);
    }
  }

  // Connect to trace for logging SNR and pathloss
  Config::Connect("/NodeList/*/DeviceList/*/TxPhy/SignalArrival",
                  MakeBoundCallback(&SimulationHelper::LogPathlossAndSNR), devices.Get(0)->GetPhy());

  // Schedule simulation stop
  Simulator::Stop(Seconds(10.0));

  // Output file header
  std::ofstream outFile("pathloss_snr_log.csv");
  outFile << "Time(s),AvgSNR(dB),Pathloss(dB)" << std::endl;
  outFile.close();

  // Run simulation
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}