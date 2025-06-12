#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 6 STAs and 3 APs
    NodeContainer staNodes[3];
    NodeContainer apNodes;
    staNodes[0].Create(2);
    staNodes[1].Create(2);
    staNodes[2].Create(2);
    apNodes.Create(3);

    // Remote server node
    NodeContainer serverNode;
    serverNode.Create(1);

    // WiFi configuration
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHz);
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    WifiChannelHelper channel = WifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    // MAC layer configuration for APs
    Ssid ssid1 = Ssid("wifi-network-1");
    Ssid ssid2 = Ssid("wifi-network-2");
    Ssid ssid3 = Ssid("wifi-network-3");

    WifiMacHelper mac;
    NetDeviceContainer apDevices[3], staDevices[3];

    // Install AP and STA devices
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid1),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    apDevices[0] = wifi.Install(phy, mac, apNodes.Get(0));

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid1),
                "ActiveProbing", BooleanValue(false));
    staDevices[0] = wifi.Install(phy, mac, staNodes[0]);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid2),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    apDevices[1] = wifi.Install(phy, mac, apNodes.Get(1));

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid2),
                "ActiveProbing", BooleanValue(false));
    staDevices[1] = wifi.Install(phy, mac, staNodes[1]);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid3),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    apDevices[2] = wifi.Install(phy, mac, apNodes.Get(2));

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid3),
                "ActiveProbing", BooleanValue(false));
    staDevices[2] = wifi.Install(phy, mac, staNodes[2]);

    // Mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(staNodes[0]);
    mobility.Install(staNodes[1]);
    mobility.Install(staNodes[2]);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);
    mobility.Install(serverNode);

    // Internet stack
    InternetStackHelper stack;
    stack.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces[3], apInterfaces[3], serverInterface;

    for (uint8_t i = 0; i < 3; ++i) {
        staInterfaces[i] = address.Assign(staDevices[i]);
        address.NewNetwork();
        apInterfaces[i] = address.Assign(apDevices[i]);
        address.NewNetwork();
    }

    // Point-to-point link between APs and server
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer p2pDevices[3];
    for (uint8_t i = 0; i < 3; ++i) {
        p2pDevices[i] = p2p.Install(apNodes.Get(i), serverNode.Get(0));
    }

    Ipv4InterfaceContainer p2pInterfaces[3];
    for (uint8_t i = 0; i < 3; ++i) {
        address.SetBase("192.168." + std::to_string(i) + ".0", "255.255.255.0");
        p2pInterfaces[i] = address.Assign(p2pDevices[i]);
        address.NewNetwork();
    }

    // Set default routes
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP Echo Server on the remote server
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(serverNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP Clients from each STA to server
    for (uint8_t i = 0; i < 3; ++i) {
        for (uint32_t j = 0; j < staNodes[i].GetN(); ++j) {
            UdpEchoClientHelper echoClient(p2pInterfaces[i].GetAddress(1), port);
            echoClient.SetAttribute("MaxPackets", UintegerValue(5));
            echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
            echoClient.SetAttribute("PacketSize", UintegerValue(1024));

            ApplicationContainer clientApp = echoClient.Install(staNodes[i].Get(j));
            clientApp.Start(Seconds(2.0 + i + j));
            clientApp.Stop(Seconds(10.0));
        }
    }

    // Animation output
    AnimationInterface anim("wifi_mobility.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}