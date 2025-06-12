#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3);

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetChannel(wifiChannel.Create());

    Wifi80211pMacHelper wifiMac;
    Wifi80211pHelper wifi;
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    // Set initial positions and velocities
    Ptr<ConstantVelocityMobilityModel> node0 = DynamicCast<ConstantVelocityMobilityModel>(nodes.Get(0)->GetObject<MobilityModel>());
    node0->SetPosition(Vector(0.0, 0.0, 0.0));
    node0->SetVelocity(Vector(20.0, 0.0, 0.0)); // 20 m/s along X-axis

    Ptr<ConstantVelocityMobilityModel> node1 = DynamicCast<ConstantVelocityMobilityModel>(nodes.Get(1)->GetObject<MobilityModel>());
    node1->SetPosition(Vector(-50.0, 0.0, 0.0));
    node1->SetVelocity(Vector(15.0, 0.0, 0.0)); // 15 m/s along X-axis

    Ptr<ConstantVelocityMobilityModel> node2 = DynamicCast<ConstantVelocityMobilityModel>(nodes.Get(2)->GetObject<MobilityModel>());
    node2->SetPosition(Vector(-100.0, 0.0, 0.0));
    node2->SetVelocity(Vector(10.0, 0.0, 0.0)); // 10 m/s along X-axis

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

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