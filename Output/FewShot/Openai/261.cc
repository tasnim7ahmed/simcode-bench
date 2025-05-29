#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wave-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging for debugging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create vehicle nodes
    NodeContainer vehicles;
    vehicles.Create(3);

    // Install WAVE/802.11p devices
    YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wavePhy = YansWifiPhyHelper::Default();
    wavePhy.SetChannel(waveChannel.Create());
    NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default();
    Wifi80211pHelper waveHelper = Wifi80211pHelper::Default();

    NetDeviceContainer devices = waveHelper.Install(wavePhy, waveMac, vehicles);

    // Assign mobility and constant velocities
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(10.0),
                                 "DeltaY", DoubleValue(0.0),
                                 "GridWidth", UintegerValue(3),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.Install(vehicles);

    Ptr<ConstantVelocityMobilityModel> v0 = vehicles.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    Ptr<ConstantVelocityMobilityModel> v1 = vehicles.Get(1)->GetObject<ConstantVelocityMobilityModel>();
    Ptr<ConstantVelocityMobilityModel> v2 = vehicles.Get(2)->GetObject<ConstantVelocityMobilityModel>();

    v0->SetVelocity(Vector(20.0, 0.0, 0.0));
    v1->SetVelocity(Vector(15.0, 0.0, 0.0));
    v2->SetVelocity(Vector(25.0, 0.0, 0.0));

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(vehicles);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP Echo Server on the last vehicle
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(vehicles.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP Echo Client on the first vehicle
    UdpEchoClientHelper echoClient(interfaces.GetAddress(2), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = echoClient.Install(vehicles.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}