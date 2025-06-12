#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetWaveSimulation");

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes for three vehicles
    NodeContainer vehicles;
    vehicles.Create(3);

    // Setup mobility models
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicles);

    // Set positions and velocities
    Ptr<MobilityModel> mob0 = vehicles.Get(0)->GetObject<MobilityModel>();
    mob0->SetPosition(Vector(0.0, 0.0, 0.0));
    DynamicCast<ConstantVelocityMobilityModel>(mob0)->SetVelocity(Vector(20.0, 0.0, 0.0));

    Ptr<MobilityModel> mob1 = vehicles.Get(1)->GetObject<MobilityModel>();
    mob1->SetPosition(Vector(-50.0, 0.0, 0.0));
    DynamicCast<ConstantVelocityMobilityModel>(mob1)->SetVelocity(Vector(15.0, 0.0, 0.0));

    Ptr<MobilityModel> mob2 = vehicles.Get(2)->GetObject<MobilityModel>();
    mob2->SetPosition(Vector(50.0, 0.0, 0.0));
    DynamicCast<ConstantVelocityMobilityModel>(mob2)->SetVelocity(Vector(25.0, 0.0, 0.0));

    // Configure WAVE (802.11p) PHY and MAC layers
    YansWavePhyHelper wavePhy = YansWavePhyHelper::Default();
    NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default();

    // Install internet stack with WAVE protocol
    InternetStackHelper internet;
    WaveNetDeviceHelper waveHelper = WaveNetDeviceHelper::Default();
    NetDeviceContainer waveDevices;

    for (uint32_t i = 0; i < vehicles.GetN(); ++i) {
        NodeContainer nodeContainer = NodeContainer(vehicles.Get(i));
        waveDevices.Add(waveHelper.Install(wavePhy, waveMac, nodeContainer));
    }

    internet.Install(vehicles);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(waveDevices);

    // Setup applications
    uint16_t port = 9; // UDP echo port

    // Server on vehicle 2
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApps = server.Install(vehicles.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Client on vehicle 0 sending to vehicle 2
    UdpEchoClientHelper client(interfaces.GetAddress(2), port);
    client.SetAttribute("MaxPackets", UintegerValue(5));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(vehicles.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Start simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}