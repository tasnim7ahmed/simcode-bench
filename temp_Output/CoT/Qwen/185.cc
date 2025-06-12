#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiNetworkSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer apNodes;
    apNodes.Create(3);

    NodeContainer staNodes;
    staNodes.Create(6);

    NodeContainer remoteServer;
    remoteServer.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::AarfwiFiRemoteStationManager");

    WifiMacHelper mac;
    Ssid ssid1 = Ssid("wifi-network-1");
    Ssid ssid2 = Ssid("wifi-network-2");
    Ssid ssid3 = Ssid("wifi-network-3");

    NetDeviceContainer apDevices1, apDevices2, apDevices3;
    NetDeviceContainer staDevices1, staDevices2, staDevices3;

    // Assign first 2 STAs to AP1
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    apDevices1 = wifi.Install(phy, apNodes.Get(0));
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1), "ActiveProbing", BooleanValue(false));
    staDevices1 = wifi.Install(phy, NodeContainer(staNodes.Get(0), staNodes.Get(1)));

    // Assign next 2 STAs to AP2
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
    apDevices2 = wifi.Install(phy, apNodes.Get(1));
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2), "ActiveProbing", BooleanValue(false));
    staDevices2 = wifi.Install(phy, NodeContainer(staNodes.Get(2), staNodes.Get(3)));

    // Assign remaining 2 STAs to AP3
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid3));
    apDevices3 = wifi.Install(phy, apNodes.Get(2));
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid3), "ActiveProbing", BooleanValue(false));
    staDevices3 = wifi.Install(phy, NodeContainer(staNodes.Get(4), staNodes.Get(5)));

    // Create point-to-point links between APs and the server
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevicesAp1, p2pDevicesAp2, p2pDevicesAp3;
    p2pDevicesAp1 = p2p.Install(apNodes.Get(0), remoteServer.Get(0));
    p2pDevicesAp2 = p2p.Install(apNodes.Get(1), remoteServer.Get(0));
    p2pDevicesAp3 = p2p.Install(apNodes.Get(2), remoteServer.Get(0));

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;

    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces1 = address.Assign(apDevices1);
    address.Assign(staDevices1);

    address.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces2 = address.Assign(apDevices2);
    address.Assign(staDevices2);

    address.SetBase("192.168.3.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces3 = address.Assign(apDevices3);
    address.Assign(staDevices3);

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfacesAp1 = address.Assign(p2pDevicesAp1);
    Ipv4InterfaceContainer p2pInterfacesAp2 = address.Assign(p2pDevicesAp2);
    Ipv4InterfaceContainer p2pInterfacesAp3 = address.Assign(p2pDevicesAp3);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup UDP Echo Server on the remote server
    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApps = server.Install(remoteServer.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Setup UDP Echo Clients from all stations
    for (uint32_t i = 0; i < staNodes.GetN(); ++i) {
        UdpEchoClientHelper client(p2pInterfacesAp1.GetAddress(1), port);
        client.SetAttribute("MaxPackets", UintegerValue(10));
        client.SetAttribute("Interval", TimeValue(Seconds(1.)));
        client.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = client.Install(staNodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
    }

    // Mobility models for STAs
    MobilityHelper mobilitySta;
    mobilitySta.SetPositionAllocator("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue(0.0),
                                     "MinY", DoubleValue(0.0),
                                     "DeltaX", DoubleValue(10.0),
                                     "DeltaY", DoubleValue(10.0),
                                     "GridWidth", UintegerValue(6),
                                     "LayoutType", StringValue("RowFirst"));
    mobilitySta.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                 "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobilitySta.Install(staNodes);

    MobilityHelper mobilityAp;
    mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityAp.Install(apNodes);
    mobilityAp.Install(remoteServer);

    // Animation
    AnimationInterface anim("wifi_network.xml");
    anim.SetMobilityPollInterval(Seconds(0.1));

    // Optional: Color nodes in animation
    for (uint32_t i = 0; i < apNodes.GetN(); ++i) {
        anim.UpdateNodeDescription(apNodes.Get(i), "AP");
        anim.UpdateNodeColor(apNodes.Get(i), 255, 0, 0); // Red
    }
    for (uint32_t i = 0; i < staNodes.GetN(); ++i) {
        anim.UpdateNodeDescription(staNodes.Get(i), "STA");
        anim.UpdateNodeColor(staNodes.Get(i), 0, 0, 255); // Blue
    }
    anim.UpdateNodeDescription(remoteServer.Get(0), "Server");
    anim.UpdateNodeColor(remoteServer.Get(0), 0, 255, 0); // Green

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}