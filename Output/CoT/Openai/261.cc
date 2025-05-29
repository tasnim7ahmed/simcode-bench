#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wave-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set up logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes (vehicles)
    NodeContainer nodes;
    nodes.Create(3);

    // Setup 802.11p (WAVE) devices and channel
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    NqosWaveMacHelper mac = NqosWaveMacHelper::Default();
    Wifi80211pHelper wave = Wifi80211pHelper::Default();
    wave.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("OfdmRate6MbpsBW10MHz"),
                                "ControlMode", StringValue("OfdmRate6MbpsBW10MHz"));

    NetDeviceContainer devices = wave.Install(phy, mac, nodes);

    // Mobility: ConstantVelocityMobilityModel, set at different speeds
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    Ptr<ConstantVelocityMobilityModel> mob0 = nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    Ptr<ConstantVelocityMobilityModel> mob1 = nodes.Get(1)->GetObject<ConstantVelocityMobilityModel>();
    Ptr<ConstantVelocityMobilityModel> mob2 = nodes.Get(2)->GetObject<ConstantVelocityMobilityModel>();

    mob0->SetPosition(Vector(0.0, 0.0, 0.0));
    mob1->SetPosition(Vector(5.0, 0.0, 0.0));
    mob2->SetPosition(Vector(10.0, 0.0, 0.0));

    mob0->SetVelocity(Vector(20.0, 0.0, 0.0)); // 20 m/s
    mob1->SetVelocity(Vector(15.0, 0.0, 0.0)); // 15 m/s
    mob2->SetVelocity(Vector(25.0, 0.0, 0.0)); // 25 m/s

    // Internet stack and IPv4 assignment
    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP Echo Server on last vehicle
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Echo Client on first vehicle, targets the last vehicle
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