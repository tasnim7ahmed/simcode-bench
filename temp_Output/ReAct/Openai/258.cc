#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 3 nodes (vehicles)
    NodeContainer nodes;
    nodes.Create(3);

    // Configure the WAVE device and channel (802.11p)
    YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default();
    YansWavePhyHelper wavePhy = YansWavePhyHelper::Default();
    wavePhy.SetChannel(waveChannel.Create());
    NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default();
    WaveHelper waveHelper = WaveHelper::Default();
    waveHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
        "DataMode", StringValue("OfdmRate6MbpsBW10MHz"),
        "ControlMode", StringValue("OfdmRate6MbpsBW10MHz"));

    NetDeviceContainer devices = waveHelper.Install(wavePhy, waveMac, nodes);

    // Mobility: ConstantVelocityMobilityModel for each vehicle
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(10.0),
                                 "DeltaY", DoubleValue(0.0),
                                 "GridWidth", UintegerValue(3),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.Install(nodes);

    Ptr<ConstantVelocityMobilityModel> mob0 = nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    Ptr<ConstantVelocityMobilityModel> mob1 = nodes.Get(1)->GetObject<ConstantVelocityMobilityModel>();
    Ptr<ConstantVelocityMobilityModel> mob2 = nodes.Get(2)->GetObject<ConstantVelocityMobilityModel>();

    if (mob0)
        mob0->SetVelocity(Vector(10.0, 0.0, 0.0)); // 10 m/s along X
    if (mob1)
        mob1->SetVelocity(Vector(15.0, 0.0, 0.0)); // 15 m/s along X
    if (mob2)
        mob2->SetVelocity(Vector(20.0, 0.0, 0.0)); // 20 m/s along X

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // UDP Echo server on the last vehicle (node 2)
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);

    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Echo client on the first vehicle (node 0), send 3 packets of 512 bytes
    UdpEchoClientHelper echoClient(interfaces.GetAddress(2), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // send every 1 second
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable PCAP tracing (optional)
    wavePhy.EnablePcapAll("vanet-wave");

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}