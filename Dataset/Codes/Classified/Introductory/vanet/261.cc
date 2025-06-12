#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer vehicles;
    vehicles.Create(3); // Create 3 vehicles

    YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wavePhy = YansWifiPhyHelper::Default();
    wavePhy.SetChannel(waveChannel.Create());

    NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default();
    WaveHelper wave;
    NetDeviceContainer devices = wave.Install(wavePhy, waveMac, vehicles);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicles);

    Ptr<ConstantVelocityMobilityModel> mobility1 = vehicles.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    Ptr<ConstantVelocityMobilityModel> mobility2 = vehicles.Get(1)->GetObject<ConstantVelocityMobilityModel>();
    Ptr<ConstantVelocityMobilityModel> mobility3 = vehicles.Get(2)->GetObject<ConstantVelocityMobilityModel>();

    mobility1->SetVelocity(Vector(20, 0, 0)); // Vehicle 1 moves at 20 m/s
    mobility2->SetVelocity(Vector(15, 0, 0)); // Vehicle 2 moves at 15 m/s
    mobility3->SetVelocity(Vector(25, 0, 0)); // Vehicle 3 moves at 25 m/s

    InternetStackHelper internet;
    internet.Install(vehicles);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(vehicles.Get(2)); // Last vehicle as server
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(2), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(vehicles.Get(0)); // First vehicle as client
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
