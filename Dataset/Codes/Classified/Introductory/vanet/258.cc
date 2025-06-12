#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer vehicles;
    vehicles.Create(3); // Create 3 vehicles (nodes)

    YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wavePhy = YansWifiPhyHelper::Default();
    wavePhy.SetChannel(waveChannel.Create());

    NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default();
    Wifi80211pHelper waveHelper = Wifi80211pHelper::Default();

    NetDeviceContainer waveDevices = waveHelper.Install(wavePhy, waveMac, vehicles);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicles);

    vehicles.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(0, 0, 0));
    vehicles.Get(1)->GetObject<MobilityModel>()->SetPosition(Vector(50, 0, 0));
    vehicles.Get(2)->GetObject<MobilityModel>()->SetPosition(Vector(100, 0, 0));

    vehicles.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(10, 0, 0));
    vehicles.Get(1)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(15, 0, 0));
    vehicles.Get(2)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(20, 0, 0));

    InternetStackHelper internet;
    internet.Install(vehicles);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(waveDevices);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(vehicles.Get(2)); // Last vehicle is the server
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(2), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = echoClient.Install(vehicles.Get(0)); // First vehicle is the client
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
