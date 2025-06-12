#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes: vehicle (sender) and RSU (receiver)
    NodeContainer nodes;
    nodes.Create(2); // node 0: vehicle, node 1: RSU

    // Mobility setup
    // Vehicle moves along the x-axis
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // vehicle initial pos
    positionAlloc->Add(Vector(100.0, 0.0, 0.0)); // RSU (fixed)
    mobility.SetPositionAllocator(positionAlloc);

    // Vehicle mobility: constant velocity along X axis
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes.Get(0));
    nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(20.0,0.0,0.0)); // 20 m/s

    // RSU is stationary
    MobilityHelper mobilityRsu;
    mobilityRsu.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityRsu.Install(nodes.Get(1));

    // 802.11p (WAVE) configuration
    YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wavePhy = YansWifiPhyHelper::Default();
    wavePhy.SetChannel(waveChannel.Create());

    NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default();
    Wifi80211pHelper waveHelper = Wifi80211pHelper::Default();

    NetDeviceContainer devices = waveHelper.Install(wavePhy, waveMac, nodes);

    // Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // UDP server on RSU (node 1)
    uint16_t port = 8080;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(15.0));

    // UDP client on vehicle (node 0)
    uint32_t packetSize = 512;
    uint32_t maxPacketCount = 20;
    Time interPacketInterval = Seconds(0.5);

    UdpClientHelper udpClient(interfaces.GetAddress(1), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
    udpClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = udpClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(15.0));

    Simulator::Stop(Seconds(15.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}