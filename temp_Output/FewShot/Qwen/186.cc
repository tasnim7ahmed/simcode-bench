#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set simulation time
    double simTime = 10.0;
    uint32_t numVehicles = 5;

    // Enable logging for WAVE protocols
    LogComponentEnable("WaveBsmHelper", LOG_LEVEL_INFO);

    // Create nodes (vehicles)
    NodeContainer vehicles;
    vehicles.Create(numVehicles);

    // Install mobility model: constant velocity on a straight road
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicles);

    // Set initial positions and velocities
    for (uint32_t i = 0; i < numVehicles; ++i) {
        Ptr<Node> node = vehicles.Get(i);
        Ptr<MobilityModel> mob = node->GetObject<MobilityModel>();
        mob->SetPosition(Vector(0.0 + i * 50.0, 0.0, 0.0));
        DynamicCast<ConstantVelocityMobilityModel>(mob)->SetVelocity(Vector(20.0, 0.0, 0.0)); // m/s
    }

    // Setup wireless channel using WAVE/802.11p
    WaveHelper waveHelper = WaveHelper::Default();
    NetDeviceContainer devices = waveHelper.Install(vehicles);

    // Setup BSM application for V2V communication
    WaveBsmHelper bsmHelper;
    bsmHelper.SetAttribute("DataRate", DataRateValue(DataRate("256kbps")));
    bsmHelper.SetAttribute("Interval", TimeValue(MilliSeconds(100))); // Send every 100ms
    bsmHelper.Install(vehicles);

    // Setup animation output
    AnimationInterface anim("vanet_simulation.xml");
    anim.SetMobilityPollInterval(Seconds(0.1));

    // Run the simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}