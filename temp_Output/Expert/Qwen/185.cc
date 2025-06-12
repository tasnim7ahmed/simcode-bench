#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMultiApSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Enable logging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create APs and STAs
    NodeContainer apNodes;
    apNodes.Create(3);

    NodeContainer staNodes;
    staNodes.Create(6);

    // Create channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // Setup MAC and PHY for APs
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHz);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));

    Ssid ssid = Ssid("wifi-network");
    WifiMacHelper mac;
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, staNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(2.0)));

    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, apNodes);

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(4),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(staNodes);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterfaces;
    Ipv4InterfaceContainer staInterfaces;

    apInterfaces = address.Assign(apDevices);
    address.NewNetwork();
    staInterfaces = address.Assign(staDevices);

    // Connect to remote server via point-to-point link
    NodeContainer p2pNodes;
    p2pNodes.Add(apNodes.Get(0));
    p2pNodes.Create(1); // Remote server node

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevices;
    p2pDevices = p2p.Install(p2pNodes);

    Ipv4AddressHelper p2pAddr;
    p2pAddr.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces = p2pAddr.Assign(p2pDevices);

    // Install applications on the server node (last node in p2pNodes)
    UdpServerHelper server(4000);
    ApplicationContainer serverApp = server.Install(p2pNodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpClientHelper client;
    client.SetAttribute("RemoteAddress", Ipv4AddressValue(p2pInterfaces.GetAddress(1)));
    client.SetAttribute("RemotePort", UintegerValue(4000));
    client.SetAttribute("MaxPackets", UintegerValue(1000000));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < staNodes.GetN(); ++i) {
        clientApps.Add(client.Install(staNodes.Get(i)));
        clientApps.Start(Seconds(2.0 + i * 0.5));
        clientApps.Stop(Seconds(10.0));
    }

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Animation
    AnimationInterface anim("wifi-multiap-animation.xml");
    anim.SetMobilityPollInterval(Seconds(0.1));

    // Simulation setup
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}