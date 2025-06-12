#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/buildings-module.h"
#include "ns3/config-store-module.h"
#include "ns3/applications-module.h"
#include "ns3/antenna-module.h"
#include "ns3/nr-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VehicularChannelSimulation");

class VehicularScenarioHelper {
public:
  static NetDeviceContainer SetupDevices(NodeContainer& nodes, Ptr<MobilityModel> mobility, std::string scenario);
  static void LogPathLossAndSNR(Ptr<OutputStreamWrapper> stream, Time interval, NodeContainer nodes);
};

NetDeviceContainer VehicularScenarioHelper::SetupDevices(NodeContainer& nodes, Ptr<MobilityModel> mobility, std::string scenario)
{
  // Use LTE helper to install devices with realistic channel modeling
  LteHelper lte;
  lte.SetAttribute("PathlossModel", StringValue("ns3::ThreeGppUmiPropagationLossModel"));
  if (scenario == "highway")
    lte.SetPathlossModelAttribute("Scenario", StringValue("RMa"));
  else
    lte.SetPathlossModelAttribute("Scenario", StringValue("UMi"));

  // Set frequency and bandwidth for the simulation
  lte.SetEnbDeviceAttribute("DlEarfcn", UintegerValue(100));
  lte.SetEnbDeviceAttribute("UlEarfcn", UintegerValue(18100));
  lte.SetEnbDeviceAttribute("DlBandwidth", UintegerValue(50));
  lte.SetEnbDeviceAttribute("UlBandwidth", UintegerValue(50));

  // Install antennas as Uniform Planar Arrays
  lte.SetDeviceAttribute("AntennaModel", StringValue("ns3::UniformPlanarArray"));
  lte.SetDeviceAttribute("AntennaElement", PointerValue(CreateObject<IsotropicAntennaModel>()));
  lte.SetDeviceAttribute("NumRows", UintegerValue(4));
  lte.SetDeviceAttribute("NumColumns", UintegerValue(4));
  lte.SetDeviceAttribute("AntennaVerticalSpacing", DoubleValue(0.5));
  lte.SetDeviceAttribute("AntennaHorizontalSpacing", DoubleValue(0.5));

  // Beamforming using DFT-based method
  lte.SetFfrAlgorithmType("ns3::LteFfrDistributedAlgorithm");

  NetDeviceContainer enbDevs = lte.InstallEnbDevice(nodes.Get(0), mobility);
  return enbDevs;
}

void VehicularScenarioHelper::LogPathLossAndSNR(Ptr<OutputStreamWrapper> stream, Time interval, NodeContainer nodes)
{
  for (NodeContainer::Iterator nodeIt = nodes.Begin(); nodeIt != nodes.End(); ++nodeIt) {
    Ptr<Node> node = *nodeIt;
    Ptr<NetDevice> device = node->GetDevice(0);
    Ptr<LteUePhy> phy = DynamicCast<LteUePhy>(device->GetObject<LtePhy>());
    if (phy) {
      double rsrp = phy->GetRsrp();
      double pathloss = -rsrp + 27; // assuming TX power is 27 dBm
      *stream->GetStream() << Simulator::Now().GetSeconds() << " " << pathloss << " " << phy->GetSinrDb() << std::endl;
    }
  }

  Simulator::Schedule(interval, &VehicularScenarioHelper::LogPathLossAndSNR, stream, interval, nodes);
}

int main(int argc, char* argv[])
{
  CommandLine cmd(__FILE__);
  std::string scenario = "urban"; // default scenario
  std::string outputFileName = "vehicular_simulation_output.txt";
  double simTime = 30.0; // seconds
  cmd.AddValue("scenario", "Scenario type: urban or highway", scenario);
  cmd.AddValue("outputFile", "Name of output file", outputFileName);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::BuildingsPropagationLossModel::ComputeShadowing", BooleanValue(true));
  Config::SetDefault("ns3::LteSpectrumPhy::CtrlErrorModelEnabled", BooleanValue(false));
  Config::SetDefault("ns3::LteSpectrumPhy::DataErrorModelEnabled", BooleanValue(false));

  // Create nodes
  NodeContainer nodes;
  nodes.Create(1); // Single vehicle node

  // Setup mobility
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(nodes);

  if (scenario == "urban") {
    // Urban building setup
    BuildingsHelper::Install(nodes);
  }

  // Start movement
  nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(20, 0, 0)); // 20 m/s

  // Setup LTE devices
  NetDeviceContainer devices = VehicularScenarioHelper::SetupDevices(nodes, nodes.Get(0)->GetObject<MobilityModel>(), scenario);

  // Setup logging
  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream(outputFileName);
  *stream->GetStream() << "# Time(s) PathLoss(dB) SNR(dB)" << std::endl;

  Simulator::Schedule(Seconds(0.1), &VehicularScenarioHelper::LogPathLossAndSNR, stream, Seconds(0.5), nodes);

  NS_LOG_INFO("Simulation will run for " << simTime << " seconds");
  Simulator::Stop(Seconds(simTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}