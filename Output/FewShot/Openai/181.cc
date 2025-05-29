#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer vehicles;
    vehicles.Create(2);

    // Mobility: Straight path, constant speed
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator>();
    pos->Add(Vector(0.0, 0.0, 0.0));     // Vehicle 0
    pos->Add(Vector(50.0, 0.0, 0.0));    // Vehicle 1
    mobility.SetPositionAllocator(pos);
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicles);

    vehicles.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(10.0, 0.0, 0.0));  // 10 m/s along x
    vehicles.Get(1)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(12.0, 0.0, 0.0));  // 12 m/s along x

    // DSRC/WAVE device setup
    YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default();
    YansWavePhyHelper wavePhy = YansWavePhyHelper::Default();
    wavePhy.SetChannel(waveChannel.Create());
    NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default();
    Wifi80211pHelper waveHelper = Wifi80211pHelper::Default();

    NetDeviceContainer waveDevices = waveHelper.Install(wavePhy, waveMac, vehicles);

    // Internet stack
    InternetStackHelper internet;
    internet.Install(vehicles);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.2.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(waveDevices);

    // UDP Server (vehicle 1)
    uint16_t port = 4001;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(vehicles.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    // UDP Client (vehicle 0 to vehicle 1)
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(256));
    ApplicationContainer clientApp = echoClient.Install(vehicles.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(20.0));

    // NetAnim setup
    AnimationInterface anim("vanet-dsrc.xml");
    anim.SetConstantPosition(vehicles.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(vehicles.Get(1), 50.0, 0.0);

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}