#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set simulation time
    double simTime = 10.0;
    uint32_t nodeId = 0;

    // Enable logging for WAVE protocols
    LogComponentEnable("WaveBsmHelper", LOG_LEVEL_INFO);
    LogComponentEnable("WifiPhy", LOG_LEVEL_DEBUG);

    // Create nodes (vehicles)
    NodeContainer vehicles;
    vehicles.Create(2);

    // Setup mobility model: constant velocity along a straight line
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicles);

    // Start movement
    for (NodeContainer::Iterator i = vehicles.Begin(); i != vehicles.End(); ++i) {
        Ptr<Node> node = *i;
        Ptr<MobilityModel> mob = node->GetObject<MobilityModel>();
        Ptr<ConstantVelocityMobilityModel> cvm = DynamicCast<ConstantVelocityMobilityModel>(mob);
        if (cvm) {
            cvm->SetVelocity(Vector(20.0, 0.0, 0.0)); // move at 20 m/s along x-axis
        }
    }

    // Configure DSRC/WAVE devices
    WaveHelper wave = WaveHelper::Default();
    NetDeviceContainer waveDevices = wave.Install(vehicles);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(vehicles);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(waveDevices);

    // Setup UDP Echo Server on vehicle 1
    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApps = server.Install(vehicles.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    // Setup UDP Echo Client on vehicle 0
    UdpEchoClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(5));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = client.Install(vehicles.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime));

    // Setup NetAnim visualization
    AnimationInterface anim("vanet-wave-animation.xml");
    anim.SetMobilityPollInterval(Seconds(0.1));

    // Add node descriptions
    anim.UpdateNodeDescription(vehicles.Get(0), "Vehicle-0");
    anim.UpdateNodeDescription(vehicles.Get(1), "Vehicle-1");

    // Run the simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}