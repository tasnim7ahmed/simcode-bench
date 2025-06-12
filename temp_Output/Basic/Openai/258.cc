#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3);

    YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wavePhy = YansWifiPhyHelper::Default();
    wavePhy.SetChannel(waveChannel.Create());

    NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default();
    Wifi80211pHelper waveHelper = Wifi80211pHelper::Default();
    waveHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                        "DataMode", StringValue ("OfdmRate6MbpsBW10MHz"),
                                        "ControlMode", StringValue ("OfdmRate6MbpsBW10MHz"));

    NetDeviceContainer devices = waveHelper.Install(wavePhy, waveMac, nodes);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));   // Vehicle 1
    positionAlloc->Add(Vector(5.0, 0.0, 0.0));   // Vehicle 2
    positionAlloc->Add(Vector(10.0, 0.0, 0.0));  // Vehicle 3
    mobility.SetPositionAllocator(positionAlloc);

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    // Assign different speeds along X-axis
    Ptr<ConstantVelocityMobilityModel> mob0 = nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    Ptr<ConstantVelocityMobilityModel> mob1 = nodes.Get(1)->GetObject<ConstantVelocityMobilityModel>();
    Ptr<ConstantVelocityMobilityModel> mob2 = nodes.Get(2)->GetObject<ConstantVelocityMobilityModel>();

    mob0->SetVelocity(Vector(10.0, 0.0, 0.0)); // 10 m/s
    mob1->SetVelocity(Vector(15.0, 0.0, 0.0)); // 15 m/s
    mob2->SetVelocity(Vector(20.0, 0.0, 0.0)); // 20 m/s

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP Echo Server on last vehicle (node 2), port 9
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Echo Client on first vehicle (node 0), target = node 2
    UdpEchoClientHelper echoClient(interfaces.GetAddress(2), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}