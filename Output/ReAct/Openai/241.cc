#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetWaveUdpExample");

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2); // node 0: vehicle, node 1: RSU

    // WiFi 802.11p/WAVE configuration
    YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default();
    YansWavePhyHelper wavePhy = YansWavePhyHelper::Default();
    wavePhy.SetChannel(waveChannel.Create());

    QosWaveMacHelper waveMac = QosWaveMacHelper::Default();
    WaveHelper waveHelper = WaveHelper::Default();

    NetDeviceContainer devices = waveHelper.Install(wavePhy, waveMac, nodes);

    // Mobility: Vehicle moves, RSU is stationary
    MobilityHelper mobility;

    Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator>();
    pos->Add(Vector(0.0, 0.0, 0.0)); // RSU position
    pos->Add(Vector(-100.0, 0.0, 0.0)); // Vehicle initial position
    mobility.SetPositionAllocator(pos);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes.Get(0)); // RSU

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes.Get(1)); // Vehicle
    Ptr<ConstantVelocityMobilityModel> cvmm = nodes.Get(1)->GetObject<ConstantVelocityMobilityModel>();
    cvmm->SetVelocity(Vector(20.0, 0.0, 0.0)); // 20 m/s along x-axis

    // Internet stack and IP addressing
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP server on RSU
    uint16_t serverPort = 4000;
    UdpServerHelper udpServer(serverPort);
    ApplicationContainer serverApp = udpServer.Install(nodes.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(20.0));

    // UDP client on vehicle - sends packets to RSU
    uint32_t packetSize = 512;
    uint32_t nPackets = 40;
    double interval = 0.5; // seconds (periodic)
    UdpClientHelper udpClient(interfaces.GetAddress(0), serverPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(nPackets));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
    udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApp = udpClient.Install(nodes.Get(1));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(20.0));

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}