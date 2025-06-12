#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/wave-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes for two vehicles
    NodeContainer nodes;
    nodes.Create(2);

    // Setup mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    // Position vehicle 0 at (0, 0, 0) and move it along X-axis with speed 20 m/s
    Ptr<MobilityModel> mob0 = nodes.Get(0)->GetObject<MobilityModel>();
    mob0->SetPosition(Vector(0.0, 0.0, 0.0));
    nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(20.0, 0.0, 0.0));

    // Position vehicle 1 at (100, 0, 0) and move it along X-axis with speed 15 m/s
    Ptr<MobilityModel> mob1 = nodes.Get(1)->GetObject<MobilityModel>();
    mob1->SetPosition(Vector(100.0, 0.0, 0.0));
    nodes.Get(1)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(15.0, 0.0, 0.0));

    // Configure WAVE/DSRC devices
    WaveRadioEnergyModelHelper waveEnergyModelHelper;
    NqosWaveMacHelper mac = NqosWaveMacHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    WifiChannelHelper channel = WifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    InternetStackHelper stack;
    stack.Install(nodes);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    WifiChannelHelper wifiChannel = WifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    NetDeviceContainer devices;
    devices = phy.Install(mac, nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install UDP Echo Server on node 1
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Install UDP Echo Client on node 0
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Animation interface
    AnimationInterface anim("vanet-animation.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    // Enable pcap tracing
    phy.EnablePcap("vanet_simulation", devices);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}