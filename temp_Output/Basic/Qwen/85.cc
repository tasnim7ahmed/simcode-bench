#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/waveform-generator.h"
#include "ns3/spectrum-helper.h"
#include "ns3/vanetmobility-helper.h"
#include "ns3/antenna-module.h"
#include "ns3/buildings-module.h"
#include "ns3/config-store-module.h"
#include "ns3/position-allocator.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VehicularChannelSimulation");

class VehicularScenario {
public:
  VehicularScenario();
  void RunSimulation();
private:
  void SetupNodes();
  void SetupMobility();
  void SetupAntennas();
  void SetupChannel();
  void LogMeasurements();
  void WriteToFile();

  NodeContainer m_vehicles;
  MobilityHelper m_mobility;
  std::ofstream m_snrFile;
  std::ofstream m_pathlossFile;
};

VehicularScenario::VehicularScenario() {
  m_snrFile.open("snr_output.txt");
  m_pathlossFile.open("pathloss_output.txt");
}

void VehicularScenario::RunSimulation() {
  SetupNodes();
  SetupMobility();
  SetupAntennas();
  SetupChannel();
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  WriteToFile();
  Simulator::Destroy();
}

void VehicularScenario::SetupNodes() {
  m_vehicles.Create(5); // Create 5 vehicle nodes
}

void VehicularScenario::SetupMobility() {
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::VanetMobilityModel");
  mobility.Install(m_vehicles);
  
  // Urban scenario example: set position allocator for city environment
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 1.5));
  positionAlloc->Add(Vector(20.0, 0.0, 1.5));
  positionAlloc->Add(Vector(40.0, 0.0, 1.5));
  positionAlloc->Add(Vector(60.0, 0.0, 1.5));
  positionAlloc->Add(Vector(80.0, 0.0, 1.5));

  mobility.SetPositionAllocator(positionAlloc);
  mobility.Install(m_vehicles);
}

void VehicularScenario::SetupAntennas() {
  for (NodeContainer::Iterator i = m_vehicles.Begin(); i != m_vehicles.End(); ++i) {
    Ptr<UniformPlanarArray> antenna = CreateObject<UniformPlanarArray>();
    antenna->SetNumElements(4, 4); // 4x4 UPA
    antenna->SetAntennaElement(Ptr<IsotropicAntennaModel>(new IsotropicAntennaModel()));
    (*i)->GetObject<MobilityModel>()->SetAntenna(antenna);
  }
}

void VehicularScenario::SetupChannel() {
  Config::SetDefault("ns3::ThreeGppChannelModel::Scenario", StringValue("UMa"));
  Config::SetDefault("ns3::ThreeGppChannelModel::Frequency", DoubleValue(5.9e9)); // 5.9 GHz
  Config::SetDefault("ns3::ThreeGppChannelModel::TxHeight", DoubleValue(1.5));
  Config::SetDefault("ns3::ThreeGppChannelModel::RxHeight", DoubleValue(1.5));
  Config::SetDefault("ns3::ThreeGppChannelModel::NLosEnabled", BooleanValue(true));

  SpectrumChannelHelper channelHelper = SpectrumChannelHelper::Default();
  channelHelper.SetChannel("ns3::ThreeGppSpectrumChannel", "Scenario", StringValue("UMa"));

  NetDeviceContainer devices = channelHelper.Install(m_vehicles);

  // Beamforming using DFT-based methods
  for (auto it = m_vehicles.Begin(); it != m_vehicles.End(); ++it) {
    Ptr<NetDevice> device = (*it)->GetDevice(0);
    Ptr<SpectrumPhy> phy = device->GetPhy();
    if (phy) {
      Ptr<BeamformingVectorCalculator> bfCalc = CreateObject<DftBasedBeamforming>();
      phy->SetBeamformingVectorCalculator(bfCalc);
    }
  }

  // Logging SNR and pathloss over time
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::SpectrumChannel/PathLoss", MakeCallback(&VehicularScenario::LogMeasurements, this));
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::SpectrumChannel/SNR", MakeCallback(&VehicularScenario::LogMeasurements, this));
}

void VehicularScenario::LogMeasurements() {
  double avgSnr = 0.0;
  double avgPathloss = 0.0;
  uint32_t count = 0;

  for (NodeContainer::Iterator it = m_vehicles.Begin(); it != m_vehicles.End(); ++it) {
    Ptr<MobilityModel> mobility = (*it)->GetObject<MobilityModel>();
    for (NodeContainer::Iterator jt = m_vehicles.Begin(); jt != m_vehicles.End(); ++jt) {
      if (it == jt) continue;
      Ptr<MobilityModel> otherMobility = (*jt)->GetObject<MobilityModel>();
      double snr = 0.0; // Placeholder - actual value should come from trace source
      double pathloss = mobility->GetDistanceFrom(otherMobility) * 2.0; // Simplified model

      avgSnr += snr;
      avgPathloss += pathloss;
      count++;
    }
  }

  if (count > 0) {
    avgSnr /= static_cast<double>(count);
    avgPathloss /= static_cast<double>(count);
  }

  Time now = Simulator::Now();
  m_snrFile << now.GetSeconds() << " " << avgSnr << std::endl;
  m_pathlossFile << now.GetSeconds() << " " << avgPathloss << std::endl;

  Simulator::Schedule(Seconds(0.1), &VehicularScenario::LogMeasurements, this);
}

void VehicularScenario::WriteToFile() {
  m_snrFile.close();
  m_pathlossFile.close();
}

int main(int argc, char *argv[]) {
  VehicularScenario scenario;
  scenario.RunSimulation();
  return 0;
}