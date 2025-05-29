#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-helper.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    NqosWaveMacHelper mac = NqosWaveMacHelper::Default();
    Wifi80211pHelper waveHelper = Wifi80211pHelper::Default();

    NetDeviceContainer devices = waveHelper.Install(phy, mac, nodes);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
    posAlloc->Add(Vector(0.0, 0.0, 0.0));  // Vehicle 1
    posAlloc->Add(Vector(0.0, 50.0, 0.0)); // Vehicle 2
    mobility.SetPositionAllocator(posAlloc);

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(20.0, 0.0, 0.0));
    nodes.Get(1)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(20.0, 0.0, 0.0));

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    uint16_t port = 4000;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AnimationInterface anim("vanet-dsrc.xml");
    anim.SetConstantPosition(nodes.Get(0), 0.0, 0.0, 0.0);
    anim.SetConstantPosition(nodes.Get(1), 0.0, 50.0, 0.0);

    phy.EnablePcapAll("vanet-dsrc");

    Simulator::Stop(Seconds(12.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}