#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WaveVanetSimulation");

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes: vehicle and RSU
    NodeContainer nodes;
    nodes.Create(2);

    Ptr<Node> vehicle = nodes.Get(0);
    Ptr<Node> rsu = nodes.Get(1);

    // Install Mobility Model
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));    // Vehicle start
    positionAlloc->Add(Vector(100.0, 0.0, 0.0));  // RSU position
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    Ptr<ConstantVelocityMobilityModel> vehMob = vehicle->GetObject<ConstantVelocityMobilityModel>();
    vehMob->SetVelocity(Vector(20.0, 0.0, 0.0)); // 20 m/s along X

    Ptr<ConstantVelocityMobilityModel> rsuMob = rsu->GetObject<ConstantVelocityMobilityModel>();
    rsuMob->SetVelocity(Vector(0.0, 0.0, 0.0)); // Stationary

    // Configure 802.11p (WAVE) devices
    YansWifiPhyHelper wavePhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default();
    wavePhy.SetChannel(waveChannel.Create());
    wavePhy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11);

    NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default();
    WaveHelper waveHelper = WaveHelper::Default();
    waveHelper.SetStandard(WIFI_PHY_STANDARD_80211p);

    NetDeviceContainer devices = waveHelper.Install(wavePhy, waveMac, nodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // UDP Server (on RSU)
    uint16_t serverPort = 5000;
    UdpServerHelper udpServer(serverPort);
    ApplicationContainer serverApp = udpServer.Install(rsu);
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(15.0));

    // UDP Client (on vehicle)
    uint32_t packetSize = 512;
    uint32_t nPackets = 1000;
    Time interPacketInterval = Seconds(0.5); // Periodic: every 0.5s
    UdpClientHelper udpClient(interfaces.GetAddress(1), serverPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(nPackets));
    udpClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApp = udpClient.Install(vehicle);
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(15.0));

    // Enable PCAP tracing for analysis
    wavePhy.EnablePcap("vanet_wave_rsu", devices.Get(1));
    wavePhy.EnablePcap("vanet_wave_vehicle", devices.Get(0));

    Simulator::Stop(Seconds(16.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}