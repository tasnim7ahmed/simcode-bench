#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/config-store-module.h"
#include "ns3/log.h"
#include "ns3/buildings-module.h"
#include "ns3/netanim-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

#include <iostream>
#include <fstream>
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VehicularSimulation");

int main(int argc, char *argv[]) {
  // Enable logging
  LogComponentEnable("VehicularSimulation", LOG_LEVEL_INFO);

  // Configure simulation parameters
  uint16_t numberOfEnb = 2;
  uint16_t numberOfUe = 10;
  double simTime = 10.0;
  double interEnbDistance = 200.0; // meters
  bool enablePcap = false;
  std::string scenario = "urban"; // urban or highway
  std::string outputFile = "vehicular_simulation_results.txt";

  // Command line arguments
  CommandLine cmd;
  cmd.AddValue("numberOfEnb", "Number of eNBs", numberOfEnb);
  cmd.AddValue("numberOfUe", "Number of UEs", numberOfUe);
  cmd.AddValue("simTime", "Simulation time (s)", simTime);
  cmd.AddValue("interEnbDistance", "Inter-eNB distance (m)", interEnbDistance);
  cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue("scenario", "Simulation scenario (urban or highway)", scenario);
  cmd.AddValue("outputFile", "Output file name", outputFile);
  cmd.Parse(argc, argv);

  // Create nodes: eNBs and UEs
  NodeContainer enbNodes;
  enbNodes.Create(numberOfEnb);

  NodeContainer ueNodes;
  ueNodes.Create(numberOfUe);

  // Configure LTE helper
  LteHelper lteHelper;
  lteHelper.SetEnbAntennaModelType("ns3::CosineAntennaModel");
  lteHelper.SetUeAntennaModelType("ns3::CosineAntennaModel");

  // Set pathloss model based on scenario
  if (scenario == "urban") {
    lteHelper.SetPathlossModelType("ns3::UrbanUmiPropagationLossModel");
  } else if (scenario == "highway") {
    lteHelper.SetPathlossModelType("ns3::BuildingsPropagationLossModel"); // Use buildings model for now
  } else {
    std::cerr << "Invalid scenario: " << scenario << std::endl;
    return 1;
  }

  // Create eNB devices
  NetDeviceContainer enbDevs;
  enbDevs = lteHelper.InstallEnbDevice(enbNodes);

  // Create UE devices
  NetDeviceContainer ueDevs;
  ueDevs = lteHelper.InstallUeDevice(ueNodes);

  // Configure mobility models
  MobilityHelper mobilityEnb;
  mobilityEnb.SetPositionAllocator("ns3::GridPositionAllocator",
                                    "MinX", DoubleValue(0.0),
                                    "MinY", DoubleValue(0.0),
                                    "DeltaX", DoubleValue(interEnbDistance),
                                    "DeltaY", DoubleValue(0.0),
                                    "GridWidth", UintegerValue(numberOfEnb),
                                    "LayoutType", StringValue("RowFirst"));
  mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobilityEnb.Install(enbNodes);

  MobilityHelper mobilityUe;
  if (scenario == "urban") {
      mobilityUe.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                  "Mode", StringValue("Time"),
                                  "Time", StringValue("2s"),
                                  "Speed", StringValue("5m/s"),
                                  "Bounds", RectangleValue(Rectangle(0, 200, 0, 200)));
  } else {
      mobilityUe.SetMobilityModel("ns3::ConstantPositionMobilityModel"); // Use constant position for now
  }
  mobilityUe.Install(ueNodes);

  // Attach UEs to eNBs
  for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
    lteHelper.Attach(ueDevs.Get(i), enbDevs.Get(i % numberOfEnb)); // Assign UEs to eNBs in round-robin fashion
  }

    // Activate default EPS bearer
  for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        Ptr<NetDevice> ueLteDev = ueDevs.Get (i);
        Ptr<LteUeNetDevice> ueLteNetDev = ueLteDev->GetObject<LteUeNetDevice> ();
        Ipv4Address ipAddr = Ipv4Address ("10.1.1." + std::to_string(i+1));
        ueLteNetDev->AssignIpv4Address (ipAddr);
        ueLteNetDev->SetMtu (1500);
        lteHelper.ActivateDedicatedBearer (ueDevs.Get(i), EpsBearer (EpsBearer::NGBR_Video_Streaming), TdmaOptions());
    }

  // Schedule measurement and logging
  Simulator::Schedule(Seconds(0.1), [&]() {
    std::ofstream ofs(outputFile, std::ios::app);
    ofs << "Time (s),UE ID,eNB ID,SNR (dB),Pathloss (dB)" << std::endl;
    ofs.close();
  });

  Simulator::Schedule(Seconds(1.0), [&]() {
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
      Ptr<MobilityModel> ueMobility = ueNodes.Get(i)->GetObject<MobilityModel>();
      Vector uePosition = ueMobility->GetPosition();

      for (uint32_t j = 0; j < enbNodes.GetN(); ++j) {
        Ptr<MobilityModel> enbMobility = enbNodes.Get(j)->GetObject<MobilityModel>();
        Vector enbPosition = enbMobility->GetPosition();

        // Calculate distance (approximated)
        double distance = std::sqrt(std::pow(uePosition.x - enbPosition.x, 2) +
                                     std::pow(uePosition.y - enbPosition.y, 2) +
                                     std::pow(uePosition.z - enbPosition.z, 2));

        // Calculate pathloss (using simplified model for demonstration)
        double pathlossDb = 128.1 + 37.6 * std::log10(distance / 1000.0); // Example: 3GPP TR 38.901

        // Estimate SNR (simplified - needs proper channel model integration)
        double txPowerDb = 46.0; // dBm
        double noiseFloorDb = -104.0; // dBm
        double snrDb = txPowerDb - pathlossDb - noiseFloorDb;

        // Log the results
        std::ofstream ofs(outputFile, std::ios::app);
        ofs << Simulator::Now().GetSeconds() << "," << i << "," << j << "," << snrDb << "," << pathlossDb << std::endl;
        ofs.close();

          NS_LOG_INFO("UE " << i << " eNB " << j << " SNR: " << snrDb << " Pathloss: " << pathlossDb);
      }
    }
  });

  // Run the simulation
  Simulator::Stop(Seconds(simTime));

  Simulator::Run();

  Simulator::Destroy();
  return 0;
}