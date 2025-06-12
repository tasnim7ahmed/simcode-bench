#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Simulation parameters
    double simDurationSeconds = 60.0;
    uint32_t numStas = 6;
    double areaWidth = 100.0;  // in meters
    double areaHeight = 100.0; // in meters

    // Enable packet loss and handover logging (optional)
    LogComponentEnable("WifiPhy", LOG_LEVEL_INFO);
    LogComponentEnable("StaWifiMac", LOG_LEVEL_INFO);

    // Create AP nodes
    NodeContainer apNodes;
    apNodes.Create(2);

    // Create mobile stations (STAs)
    NodeContainer staNodes;
    staNodes.Create(numStas);

    // Combine all nodes
    NodeContainer allNodes = NodeContainer(apNodes, staNodes);

    // Install Wifi and Channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

    // Set up access points
    Ssid ssid1 = Ssid("networkOne");
    Ssid ssid2 = Ssid("networkTwo");

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid1),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    NetDeviceContainer apDevices1 = wifi.Install(phy, mac, apNodes.Get(0));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid2),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    NetDeviceContainer apDevices2 = wifi.Install(phy, mac, apNodes.Get(1));

    // Set up stations with SSID scanning and roaming
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(),
                "ActiveProbing", BooleanValue(true));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces1 = address.Assign(apDevices1);
    Ipv4InterfaceContainer apInterfaces2 = address.Assign(apDevices2);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // Mobility model for STAs
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaWidth) + "]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaHeight) + "]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, areaWidth, 0, areaHeight)));
    mobility.Install(staNodes);

    // Static positions for APs
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(areaWidth / 3, areaHeight / 2, 0)); // First AP
    positionAlloc->Add(Vector(2 * areaWidth / 3, areaHeight / 2, 0)); // Second AP
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(apNodes);

    // Setup UDP echo server/client to generate traffic and observe packet loss
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps;

    for (uint32_t i = 0; i < apNodes.GetN(); ++i) {
        serverApps.Add(echoServer.Install(apNodes.Get(i)));
    }
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simDurationSeconds));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < staNodes.GetN(); ++i) {
        UdpEchoClientHelper echoClient(staInterfaces.GetAddress((i + 1) % numStas), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(4294967295u)); // Infinite packets
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));
        clientApps.Add(echoClient.Install(staNodes.Get(i)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simDurationSeconds));

    // Animation
    AnimationInterface anim("wireless_handover_simulation.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    // Run simulation
    Simulator::Stop(Seconds(simDurationSeconds));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}