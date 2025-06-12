#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Logging (optional, comment out to reduce output)
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer apNodes;
    apNodes.Create(3); // 3 APs

    NodeContainer staNodes;
    staNodes.Create(6); // 6 STAs

    NodeContainer serverNode;
    serverNode.Create(1); // Remote server

    // Wi-Fi Channel and PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Wi-Fi standard and helper
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    WifiMacHelper mac;
    Ssid ssids[3] = { Ssid("network-1"), Ssid("network-2"), Ssid("network-3") };

    NetDeviceContainer apDevices[3];
    NetDeviceContainer staDevices[3];

    // Distribute STAs: 2 per AP
    for (uint32_t i = 0; i < 3; ++i)
    {
        // AP
        mac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssids[i]));
        apDevices[i] = wifi.Install(phy, mac, apNodes.Get(i));

        // 2 STAs per AP
        mac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssids[i]),
                    "ActiveProbing", BooleanValue(false));
        NodeContainer staSubset;
        staSubset.Add(staNodes.Get(i * 2));
        staSubset.Add(staNodes.Get(i * 2 + 1));
        NetDeviceContainer staDevs = wifi.Install(phy, mac, staSubset);
        staDevices[i] = staDevs;
    }

    // Point-to-point connection from each AP to server node
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer apP2pDevices[3];
    NetDeviceContainer serverP2pDevices[3];

    // Each AP gets its own point-to-point link to the server
    for (uint32_t i = 0; i < 3; ++i)
    {
        NetDeviceContainer p2pDevs = p2p.Install(NodeContainer(apNodes.Get(i), serverNode.Get(0)));
        apP2pDevices[i] = NetDeviceContainer(p2pDevs.Get(0));
        serverP2pDevices[i] = NetDeviceContainer(p2pDevs.Get(1));
    }

    // Mobility for APs: fixed
    MobilityHelper apMobility;
    Ptr<ListPositionAllocator> apPosAlloc = CreateObject<ListPositionAllocator>();
    apPosAlloc->Add(Vector(0.0, 0.0, 0.0));
    apPosAlloc->Add(Vector(20.0, 0.0, 0.0));
    apPosAlloc->Add(Vector(40.0, 0.0, 0.0));
    apMobility.SetPositionAllocator(apPosAlloc);
    apMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    apMobility.Install(apNodes);

    // Mobility for STAs: set initial positions, then enable random walk
    MobilityHelper staMobility;
    Ptr<ListPositionAllocator> staPosAlloc = CreateObject<ListPositionAllocator>();
    // Place 2 STAs near each AP
    staPosAlloc->Add(Vector(2.0, -2.0, 0.0));
    staPosAlloc->Add(Vector(-2.0, 2.0, 0.0));
    staPosAlloc->Add(Vector(22.0, -2.0, 0.0));
    staPosAlloc->Add(Vector(18.0, 2.0, 0.0));
    staPosAlloc->Add(Vector(42.0, -2.0, 0.0));
    staPosAlloc->Add(Vector(38.0, 2.0, 0.0));
    staMobility.SetPositionAllocator(staPosAlloc);
    staMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                 "Bounds", RectangleValue(Rectangle(-20, 60, -20, 20)),
                                 "Distance", DoubleValue(10.0));
    staMobility.Install(staNodes);

    // Server node: fixed
    MobilityHelper serverMobility;
    serverMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    serverMobility.Install(serverNode);

    // Install Internet stack
    InternetStackHelper internet;
    for (uint32_t i = 0; i < 3; ++i)
    {
        internet.Install(NodeContainer(apNodes.Get(i)));
        NodeContainer staSubset;
        staSubset.Add(staNodes.Get(i * 2));
        staSubset.Add(staNodes.Get(i * 2 + 1));
        internet.Install(staSubset);
    }
    internet.Install(serverNode);

    // Assign IP addresses
    Ipv4AddressHelper ipWifi[3];
    Ipv4InterfaceContainer staInterfaces[3], apWifiInterfaces[3], apP2pInterfaces[3], serverP2pInterfaces[3];
    char base[3][11] = {"10.1.1.0","10.1.2.0","10.1.3.0"};
    for (uint32_t i = 0; i < 3; ++i)
    {
        ipWifi[i].SetBase(Ipv4Address(base[i]), "255.255.255.0");
        staInterfaces[i] = ipWifi[i].Assign(staDevices[i]);
        apWifiInterfaces[i] = ipWifi[i].Assign(apDevices[i]);

        // Point-to-point link addresses
        std::ostringstream p2pBase;
        p2pBase << "192.168." << (i+1) << ".0";
        Ipv4AddressHelper ipP2p;
        ipP2p.SetBase(Ipv4Address(p2pBase.str().c_str()), "255.255.255.0");
        apP2pInterfaces[i] = ipP2p.Assign(apP2pDevices[i]);
        serverP2pInterfaces[i] = ipP2p.Assign(serverP2pDevices[i]);
    }

    // Set up static routing at APs and server
    Ipv4StaticRoutingHelper staticRouting;
    for (uint32_t i = 0; i < 3; ++i)
    {
        // AP: route to server
        Ptr<Node> ap = apNodes.Get(i);
        Ptr<Ipv4StaticRouting> apStaticRouting = staticRouting.GetStaticRouting(ap->GetObject<Ipv4>());
        apStaticRouting->SetDefaultRoute(apP2pInterfaces[i].GetAddress(1), 1);

        // Server: route to each Wi-Fi subnet
        Ptr<Node> srv = serverNode.Get(0);
        Ptr<Ipv4StaticRouting> srvStaticRouting = staticRouting.GetStaticRouting(srv->GetObject<Ipv4>());
        srvStaticRouting->AddNetworkRouteTo(Ipv4Address(base[i]), Ipv4Mask("255.255.255.0"), serverP2pInterfaces[i].GetAddress(0), 1);
    }

    // UDP server on remote node, listening on port 4000
    uint16_t udpPort = 4000;
    UdpServerHelper udpServer(udpPort);
    ApplicationContainer serverApps = udpServer.Install(serverNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(20.0));

    // UDP clients: 1 per STA -> send to server
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < staNodes.GetN(); ++i)
    {
        // Each STA will use the corresponding AP's P2P to find IP
        Ipv4Address serverIp = serverP2pInterfaces[i / 2].GetAddress(1);

        UdpClientHelper udpClient(serverIp, udpPort);
        udpClient.SetAttribute("MaxPackets", UintegerValue(20));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0 + 0.2 * i)));
        udpClient.SetAttribute("PacketSize", UintegerValue(512));
        clientApps.Add(udpClient.Install(staNodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    // Enable NetAnim output
    AnimationInterface anim("wifi-multi-ap.xml");
    // Assign some colors for APs, STAs, and Server
    anim.UpdateNodeColor(serverNode.Get(0), 0, 0, 255); // Server blue
    for (uint32_t i = 0; i < apNodes.GetN(); ++i)
    {
        anim.UpdateNodeColor(apNodes.Get(i), 255, 0, 0); // APs red
    }
    for (uint32_t i = 0; i < staNodes.GetN(); ++i)
    {
        anim.UpdateNodeColor(staNodes.Get(i), 0, 255, 0); // STAs green
    }

    // Run simulation
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}