#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/wifi-80211p-helper.h"
#include "ns3/wave-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes for three vehicles
    NodeContainer nodes;
    nodes.Create(3);

    // Setup WAVE PHY and MAC
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    Wifi80211pMacHelper mac = Wifi80211pMacHelper();
    Wifi80211pHelper wifi = Wifi80211pHelper();

    // Install internet stack
    InternetStackHelper internet;
    OlsrHelper olsr;
    Ipv4ListRoutingHelper list;
    list.Add(olsr, 10);
    internet.SetRoutingHelper(list);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set mobility models
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    // Set initial positions and velocities
    Ptr<ConstantVelocityMobilityModel> velModel;

    velModel = nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    velModel->SetPosition(Vector(0.0, 0.0, 0.0));
    velModel->SetVelocity(Vector(20.0, 0.0, 0.0));

    velModel = nodes.Get(1)->GetObject<ConstantVelocityMobilityModel>();
    velModel->SetPosition(Vector(-50.0, 0.0, 0.0));
    velModel->SetVelocity(Vector(15.0, 0.0, 0.0));

    velModel = nodes.Get(2)->GetObject<ConstantVelocityMobilityModel>();
    velModel->SetPosition(Vector(100.0, 0.0, 0.0));
    velModel->SetVelocity(Vector(25.0, 0.0, 0.0));

    // Server application on node 2 (last vehicle)
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Client application on node 0 (first vehicle)
    UdpEchoClientHelper echoClient(interfaces.GetAddress(2), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // Simulation setup
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}