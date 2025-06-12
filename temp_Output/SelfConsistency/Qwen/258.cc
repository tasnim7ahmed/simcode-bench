#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/wave-module.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetWifi11pExample");

int main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes for three vehicles
    NodeContainer nodes;
    nodes.Create(3);

    // Mobility setup
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    // Assign different velocities along X-axis
    DynamicCast<ConstantVelocityMobilityModel>(nodes.Get(0)->GetObject<MobilityModel>())->SetVelocity(Vector(20.0, 0.0, 0.0)); // Vehicle 1
    DynamicCast<ConstantVelocityMobilityModel>(nodes.Get(1)->GetObject<MobilityModel>())->SetVelocity(Vector(30.0, 0.0, 0.0)); // Vehicle 2
    DynamicCast<ConstantVelocityMobilityModel>(nodes.Get(2)->GetObject<MobilityModel>())->SetVelocity(Vector(10.0, 0.0, 0.0)); // Vehicle 3

    // Setup WAVE PHY and MAC
    YansWavePhyHelper wavePhy = YansWavePhyHelper::Default();
    NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default();

    WaveHelper waveHelper = WaveHelper::Default();
    NetDeviceContainer devices = waveHelper.Install(wavePhy, waveMac, nodes);

    // Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP Echo Server on the third vehicle (node index 2)
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Echo Client on the first vehicle (node index 0)
    UdpEchoClientHelper echoClient(interfaces.GetAddress(2), 9); // Send to third vehicle's IP
    echoClient.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}