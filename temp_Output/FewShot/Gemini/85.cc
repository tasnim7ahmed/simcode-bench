#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/propagation-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/log.h"
#include "ns3/buildings-module.h"
#include "ns3/netanim-module.h"
#include <fstream>
#include <iostream>
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VehicularSimulation");

double CalculateDistance(double x1, double y1, double x2, double y2) {
    return std::sqrt(std::pow(x2 - x1, 2) + std::pow(y2 - y1, 2));
}

int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    LogComponentEnable("VehicularSimulation", LOG_LEVEL_INFO);

    // Simulation parameters
    double simulationTime = 10.0; // seconds
    double updateInterval = 0.1;   // seconds
    std::string outputFileName = "vehicular_simulation_results.txt";

    // Scenario parameters
    double areaSizeX = 500.0; // meters
    double areaSizeY = 500.0; // meters
    int numberOfVehicles = 2;
    double vehicleSpeed = 20.0; // m/s

    // Channel parameters
    double carrierFrequency = 28e9; // Hz (28 GHz)
    double antennaHeight = 1.5;     // meters

    // Configure nodes (vehicles)
    NodeContainer vehicles;
    vehicles.Create(numberOfVehicles);

    // Mobility Model (Simple Random Walk)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
        "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaSizeX) + "]"),
        "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaSizeY) + "]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
        "Mode", StringValue("Time"),
        "Time", StringValue("1s"),
        "Speed", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(vehicleSpeed) + "]"),
        "Bounds", StringValue("0," + std::to_string(areaSizeX) + ",0," + std::to_string(areaSizeY) + "]"));
    mobility.Install(vehicles);

    for (int i = 0; i < numberOfVehicles; ++i) {
        Ptr<MobilityModel> mob = vehicles.Get(i)->GetObject<MobilityModel>();
        Vector3D pos = mob->GetPosition();
        NS_LOG_INFO("Vehicle " << i << " initial position: " << pos);
    }

    // Install propagation loss model (3GPP TR 38.901 - Urban Microcell)
    Ptr<LogDistancePropagationLossModel> logDistanceLossModel = CreateObject<LogDistancePropagationLossModel>();
    logDistanceLossModel->SetPathLossExponent(3.0);
    logDistanceLossModel->SetReference(1.0, 7.0);

    // Shadowing component
    Ptr<ShadowingPropagationLossModel> shadowingLossModel = CreateObject<ShadowingPropagationLossModel>();
    shadowingLossModel->SetSigma(4.0);

    // Create propagation delay model
    Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel>();

    // Output file
    std::ofstream outputFile;
    outputFile.open(outputFileName);
    outputFile << "Time (s),Vehicle1_X,Vehicle1_Y,Vehicle2_X,Vehicle2_Y,Distance,Pathloss (dB),SNR (dB)" << std::endl;

    // Simulation loop
    Simulator::Schedule(Seconds(0.0), [&]() {
        for (double time = 0.0; time <= simulationTime; time += updateInterval) {
            Simulator::Schedule(Seconds(time), [&, time]() {
                Ptr<MobilityModel> mob1 = vehicles.Get(0)->GetObject<MobilityModel>();
                Ptr<MobilityModel> mob2 = vehicles.Get(1)->GetObject<MobilityModel>();

                Vector3D pos1 = mob1->GetPosition();
                Vector3D pos2 = mob2->GetPosition();

                double distance = CalculateDistance(pos1.x, pos1.y, pos2.x, pos2.y);
                double pathloss = logDistanceLossModel->CalcRxPower(1.0, distance); // TxPower = 1.0 dBm for simplicity
                double shadowing = shadowingLossModel->GetValue(pos1, pos2);
                pathloss += shadowing;

                // Simplified SNR calculation (assuming constant noise floor)
                double snr = 10 - pathloss; //Example SNR calculation

                outputFile << time << ","
                           << pos1.x << "," << pos1.y << ","
                           << pos2.x << "," << pos2.y << ","
                           << distance << ","
                           << pathloss << ","
                           << snr << std::endl;

                NS_LOG_INFO("Time: " << time << " Vehicle 1: (" << pos1.x << ", " << pos1.y << ") Vehicle 2: (" << pos2.x << ", " << pos2.y << ") Distance: " << distance << " Pathloss: " << pathloss << " SNR: " << snr);
            });
        }
    });

    // Animation Interface
    AnimationInterface anim("vehicular_simulation.xml");
    anim.SetConstantPosition(vehicles.Get(0),50,50);
    anim.SetConstantPosition(vehicles.Get(1),100,100);

    Simulator::Stop(Seconds(simulationTime + 0.1));
    Simulator::Run();

    outputFile.close();
    Simulator::Destroy();

    return 0;
}