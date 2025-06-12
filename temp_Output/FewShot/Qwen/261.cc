#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set default WAVE MAC and PHY to use 802.11p
    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue("OfdmRate6_5MbpsBW10MHz"));
    Config::SetDefault("ns3::WaveMacHelper::Standard", EnumValue(Wave));

    // Create three vehicle nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Setup mobility with constant velocity
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");

    // Install mobility on all nodes
    mobility.Install(nodes);

    // Assign speeds
    Ptr<ConstantVelocityMobilityModel> cvmm0 = DynamicCast<ConstantVelocityMobilityModel>(nodes.Get(0)->GetObject<MobilityModel>());
    cvmm0->SetVelocity(Vector(20, 0, 0));  // 20 m/s

    Ptr<ConstantVelocityMobilityModel> cvmm1 = DynamicCast<ConstantVelocityMobilityModel>(nodes.Get(1)->GetObject<MobilityModel>());
    cvmm1->SetVelocity(Vector(15, 0, 0));  // 15 m/s

    Ptr<ConstantVelocityMobilityModel> cvmm2 = DynamicCast<ConstantVelocityMobilityModel>(nodes.Get(2)->GetObject<MobilityModel>());
    cvmm2->SetVelocity(Vector(25, 0, 0));  // 25 m/s

    // Setup WAVE (802.11p) devices
    WaveHelper waveHelper = WaveHelper::Default();
    NetDeviceContainer devices = waveHelper.Install(nodes);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses in the same subnet
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up UDP Echo Server on node 2 (last vehicle)
    uint16_t port = 9;  // UDP port number
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP Echo Client on node 0 (first vehicle)
    UdpEchoClientHelper echoClient(interfaces.GetAddress(2), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable logging for applications
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}