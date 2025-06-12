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
    vehicles.Create(3);

    YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default ();
    YansWavePhyHelper wavePhy = YansWavePhyHelper::Default ();
    wavePhy.SetChannel (waveChannel.Create ());

    NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default ();
    Wifi80211pHelper waveHelper = Wifi80211pHelper::Default ();
    waveHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("OfdmRate6MbpsBW10MHz"), "ControlMode", StringValue ("OfdmRate6MbpsBW10MHz"));

    NetDeviceContainer devices = waveHelper.Install (wavePhy, waveMac, vehicles);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(50.0, 0.0, 0.0));
    positionAlloc->Add(Vector(100.0, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);

    mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicles);

    Ptr<ConstantVelocityMobilityModel> mob0 = vehicles.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    Ptr<ConstantVelocityMobilityModel> mob1 = vehicles.Get(1)->GetObject<ConstantVelocityMobilityModel>();
    Ptr<ConstantVelocityMobilityModel> mob2 = vehicles.Get(2)->GetObject<ConstantVelocityMobilityModel>();
    mob0->SetVelocity(Vector(20.0, 0.0, 0.0));
    mob1->SetVelocity(Vector(15.0, 0.0, 0.0));
    mob2->SetVelocity(Vector(25.0, 0.0, 0.0));

    InternetStackHelper stack;
    stack.Install(vehicles);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(vehicles.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(2), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(vehicles.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(9.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}