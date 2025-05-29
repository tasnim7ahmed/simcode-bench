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
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create three vehicle nodes
    NodeContainer vehicles;
    vehicles.Create(3);

    // Configure 802.11p (WAVE) devices
    YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default();
    YansWavePhyHelper wavePhy = YansWavePhyHelper::Default();
    wavePhy.SetChannel(waveChannel.Create());
    QosWaveMacHelper waveMac = QosWaveMacHelper::Default();
    Wifi80211pHelper waveHelper = Wifi80211pHelper::Default();
    NetDeviceContainer devs = waveHelper.Install(wavePhy, waveMac, vehicles);

    // Mobility: constant velocity along X, each at different speeds
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicles);

    Ptr<ConstantVelocityMobilityModel> mob0 = vehicles.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    Ptr<ConstantVelocityMobilityModel> mob1 = vehicles.Get(1)->GetObject<ConstantVelocityMobilityModel>();
    Ptr<ConstantVelocityMobilityModel> mob2 = vehicles.Get(2)->GetObject<ConstantVelocityMobilityModel>();
    mob0->SetPosition(Vector(0.0, 0.0, 0.0));
    mob0->SetVelocity(Vector(10.0, 0.0, 0.0)); // 10 m/s for node 0
    mob1->SetPosition(Vector(5.0, 0.0, 0.0));
    mob1->SetVelocity(Vector(15.0, 0.0, 0.0)); // 15 m/s for node 1
    mob2->SetPosition(Vector(10.0, 0.0, 0.0));
    mob2->SetVelocity(Vector(20.0, 0.0, 0.0)); // 20 m/s for node 2

    // Install internet stack
    InternetStackHelper internet;
    internet.Install(vehicles);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devs);

    // UDP Echo Server on vehicle 2 (last)
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(vehicles.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Echo Client on vehicle 0 (first)
    UdpEchoClientHelper echoClient(interfaces.GetAddress(2), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = echoClient.Install(vehicles.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}