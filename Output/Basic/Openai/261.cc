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

    NodeContainer nodes;
    nodes.Create(3);

    // Install Wave devices (802.11p)
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    QosWaveMacHelper wifi80211pMac = QosWaveMacHelper::Default();
    Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default();
    NetDeviceContainer devices = wifi80211p.Install(wifiPhy, wifi80211pMac, nodes);

    // Set ConstantVelocityMobilityModel for each vehicle with unique speed
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(10.0, 0.0, 0.0));
    positionAlloc->Add(Vector(20.0, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    // Set different constant velocities
    nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(20.0, 0.0, 0.0));
    nodes.Get(1)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(15.0, 0.0, 0.0));
    nodes.Get(2)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(25.0, 0.0, 0.0));

    // Install Internet Stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IPv4 addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Install UDP Echo Server on last vehicle (node 2)
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Install UDP Echo Client on first vehicle (node 0)
    UdpEchoClientHelper echoClient(interfaces.GetAddress(2), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}