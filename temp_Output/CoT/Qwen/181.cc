#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes for two vehicles
    NodeContainer nodes;
    nodes.Create(2);

    // Setup mobility model
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    // Position the two vehicles on a straight path and assign constant velocities
    Ptr<ConstantVelocityMobilityModel> mob0 = DynamicCast<ConstantVelocityMobilityModel>(nodes.Get(0)->GetObject<MobilityModel>());
    Ptr<ConstantVelocityMobilityModel> mob1 = DynamicCast<ConstantVelocityMobilityModel>(nodes.Get(1)->GetObject<MobilityModel>());

    mob0->SetPosition(Vector(0.0, 0.0, 0.0));
    mob0->SetVelocity(Vector(20.0, 0.0, 0.0)); // Vehicle moving at 20 m/s

    mob1->SetPosition(Vector(100.0, 0.0, 0.0)); // Starts ahead of first vehicle
    mob1->SetVelocity(Vector(15.0, 0.0, 0.0)); // Vehicle moving slower at 15 m/s

    // Install WAVE/DSRC devices
    WaveRadioEnergyModelHelper waveEnergyModelHelper;
    YansWavePhyHelper wavePhy = YansWavePhyHelper::Default();
    NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default();
    WaveHelper waveHelper = WaveHelper::Default();
    NetDeviceContainer devices = waveHelper.Install(wavePhy, waveMac, nodes);
    waveEnergyModelHelper.Install(devices, nodes);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup UDP echo server application on node 1
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Setup UDP echo client application on node 0
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable PCAP tracing
    wavePhy.EnablePcap("vanet_simulation", devices);

    // Setup animation output
    AnimationInterface anim("vanet_simulation.xml");
    anim.SetMobilityPollInterval(Seconds(0.1));

    // Simulation duration
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}