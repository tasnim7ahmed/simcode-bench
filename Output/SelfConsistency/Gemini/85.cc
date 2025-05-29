#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/log.h"
#include "ns3/buildings-module.h"
#include "ns3/propagation-module.h"
#include "ns3/netanim-module.h"
#include "ns3/lte-module.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VehicularSimulation");

int main(int argc, char* argv[]) {
    // Enable logging
    LogComponentEnable("VehicularSimulation", LOG_LEVEL_INFO);

    // Set up command line arguments
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Simulation parameters
    double simulationTime = 10.0; // seconds
    double interPacketInterval = 0.01; // seconds
    uint32_t numVehicles = 10;

    // Create nodes (vehicles)
    NodeContainer vehicles;
    vehicles.Create(numVehicles);

    // Mobility model (Highway scenario example)
    MobilityHelper mobilityHighway;
    mobilityHighway.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
        "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
        "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=10.0]"),
        "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    mobilityHighway.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobilityHighway.Install(vehicles);

    for (uint32_t i = 0; i < vehicles.GetN(); ++i) {
        Ptr<ConstantVelocityMobilityModel> mob = vehicles.Get(i)->GetObject<ConstantVelocityMobilityModel>();
        mob->SetVelocity(Vector(10.0, 0.0, 0.0)); // 10 m/s along X axis
    }

    // Channel model and propagation loss
    TypeIdValue modelType = TypeIdValue(TypeId::LookupByName("ns3::ThreeGppChannelModel"));
    Config::SetDefault("ns3::LteUePhy::ChannelModel", modelType);

    // Configure pathloss model (3GPP TR 38.901 example)
    Ptr<ThreeGppChannelModel> channelModel = CreateObject<ThreeGppChannelModel>();
    channelModel->SetAttribute("Scenario", EnumValue(ThreeGppChannelModel::Umi));
    channelModel->SetAttribute("Frequency", DoubleValue(28e9)); // 28 GHz
    channelModel->SetAttribute("OutdoorEnvironment", BooleanValue(true));

    // Install the channel model
    BuildingsHelper::Install(vehicles);

    // Internet stack (needed for applications)
    InternetStackHelper internet;
    internet.Install(vehicles);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(vehicles);

    // Create Applications (e.g., on off)
    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory",
        Address(InetSocketAddress(interfaces.GetAddress(1), port)));
    onoff.SetConstantRate(DataRate("100kb/s"));

    ApplicationContainer app = onoff.Install(vehicles.Get(0));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(simulationTime + 1));

    PacketSinkHelper sink("ns3::UdpSocketFactory",
        InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(vehicles.Get(1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(simulationTime + 1));

    // Enable PCAP tracing
    // PcapHelper pcapHelper;
    // pcapHelper.EnablePcapAll("vehicular_simulation");

    // Output stream for SNR and pathloss
    std::ofstream outputFile;
    outputFile.open("vehicular_simulation_results.txt");
    outputFile << "Time (s), Vehicle ID, SNR (dB), Pathloss (dB)" << std::endl;

    // Callback function for logging SNR and pathloss
    Simulator::Schedule(Seconds(0.1), [&]() {
        for (uint32_t i = 0; i < vehicles.GetN(); ++i) {
            Ptr<MobilityModel> mobility = vehicles.Get(i)->GetObject<MobilityModel>();
            Vector position = mobility->GetPosition();

            // Example calculation (replace with actual SNR and pathloss calculation)
            double snr = 20.0;
            double pathloss = 80.0;

            outputFile << Simulator::Now().GetSeconds() << ", " << i << ", " << snr << ", " << pathloss << std::endl;
        }
    });

    // Schedule the logging callback repeatedly
    for (double time = 0.2; time <= simulationTime; time += 0.1) {
        Simulator::Schedule(Seconds(time), [&]() {
            for (uint32_t i = 0; i < vehicles.GetN(); ++i) {
                Ptr<MobilityModel> mobility = vehicles.Get(i)->GetObject<MobilityModel>();
                Vector position = mobility->GetPosition();

                // Example calculation (replace with actual SNR and pathloss calculation)
                double snr = 20.0;
                double pathloss = 80.0;

                outputFile << Simulator::Now().GetSeconds() << ", " << i << ", " << snr << ", " << pathloss << std::endl;
            }
        });
    }

    // Run simulation
    Simulator::Stop(Seconds(simulationTime + 2)); // Add some extra time
    Simulator::Run();

    // Close output file
    outputFile.close();

    Simulator::Destroy();
    return 0;
}