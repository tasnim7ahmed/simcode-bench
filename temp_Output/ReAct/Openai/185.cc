#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/wifi-mac-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMultiApStaNetAnim");

int main(int argc, char *argv[])
{
    uint32_t nAps = 3;
    uint32_t nStas = 6;

    // Create nodes
    NodeContainer apNodes;
    apNodes.Create(nAps);

    NodeContainer staNodes;
    staNodes.Create(nStas);

    NodeContainer serverNode;
    serverNode.Create(1);

    // WiFi channel and phy
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    // MACs
    WifiMacHelper mac;

    // Install devices on APs and STAs
    NetDeviceContainer apDevices;
    NetDeviceContainer staDevices;

    std::vector<NetDeviceContainer> apStaContainers(nAps);

    // Assign STAs to APs (2 per AP)
    for (uint32_t i = 0; i < nAps; ++i)
    {
        // AP MAC
        Ssid ssid = Ssid("wifi-ap-" + std::to_string(i + 1));
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDev = wifi.Install(phy, mac, apNodes.Get(i));
        apDevices.Add(apDev);

        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer staDev = wifi.Install(phy, mac, NodeContainer(staNodes.Get(i * 2), staNodes.Get(i * 2 + 1)));
        staDevices.Add(staDev);
        apStaContainers[i] = staDev;
    }

    // CSMA (wired) server <-> APs
    NodeContainer csmaNodes;
    csmaNodes.Add(serverNode);
    csmaNodes.Add(apNodes);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

    // Internet Stack
    InternetStackHelper stack;
    stack.Install(staNodes);
    stack.Install(apNodes);
    stack.Install(serverNode);

    // Assign IP addresses to Wi-Fi networks
    std::vector<Ipv4InterfaceContainer> apStaIfaces;
    Ipv4AddressHelper address;

    for (uint32_t i = 0; i < nAps; ++i)
    {
        std::ostringstream subnet;
        subnet << "10." << (i + 1) << ".0.0";
        address.SetBase(Ipv4Address(subnet.str().c_str()), "255.255.255.0");
        NetDeviceContainer devs;
        devs.Add(apDevices.Get(i));
        devs.Add(apStaContainers[i].Get(0));
        devs.Add(apStaContainers[i].Get(1));
        Ipv4InterfaceContainer iface = address.Assign(devs);
        apStaIfaces.push_back(iface);
        address.NewNetwork();
    }

    // Assign IPs to CSMA net (server + all APs)
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaIfaces = address.Assign(csmaDevices);

    // Set up default routes for STAs
    Ipv4StaticRoutingHelper staticRouting;
    for (uint32_t i = 0; i < nAps; ++i)
    {
        for (uint32_t j = 1; j <= 2; ++j)
        {
            Ptr<Node> sta = staNodes.Get(i * 2 + (j - 1));
            Ptr<Ipv4> ipv4 = sta->GetObject<Ipv4>();
            Ptr<Ipv4StaticRouting> sr = staticRouting.GetStaticRouting(ipv4);
            sr->SetDefaultRoute(apStaIfaces[i].GetAddress(0), 1);
        }
    }

    // Set up default routes for APs (to server)
    for (uint32_t i = 0; i < nAps; ++i)
    {
        Ptr<Node> ap = apNodes.Get(i);
        Ptr<Ipv4> ipv4 = ap->GetObject<Ipv4>();
        Ptr<Ipv4StaticRouting> sr = staticRouting.GetStaticRouting(ipv4);
        sr->SetDefaultRoute(csmaIfaces.GetAddress(0), 2);
    }

    // Set up default route for server: not strictly required, but for completeness
    Ptr<Ipv4> serverIpv4 = serverNode.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> serverStaticRouting = staticRouting.GetStaticRouting(serverIpv4);
    serverStaticRouting->SetDefaultRoute(Ipv4Address("0.0.0.0"), 1);

    // Mobility: place APs in a grid, STAs with mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(20.0, 80.0, 0.0));
    positionAlloc->Add(Vector(90.0, 80.0, 0.0));
    positionAlloc->Add(Vector(160.0, 80.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);

    // Server position (wired, outside Wi-Fi cluster)
    MobilityHelper serverMobility;
    serverMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    serverMobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                        "MinX", DoubleValue(90.0),
                                        "MinY", DoubleValue(0.0),
                                        "DeltaX", DoubleValue(0.0),
                                        "DeltaY", DoubleValue(0.0),
                                        "GridWidth", UintegerValue(1),
                                        "LayoutType", StringValue("RowFirst"));
    serverMobility.Install(serverNode);

    // STA initial positions: spread starting near APs
    MobilityHelper staMobility;
    staMobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                     "X", DoubleValue(20.0),
                                     "Y", DoubleValue(80.0),
                                     "Rho", StringValue("ns3::UniformRandomVariable[Min=0|Max=10]"));
    staMobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
        "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=2.0]"),
        "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
        "PositionAllocator", PointerValue(CreateObjectWithAttributes<RandomDiscPositionAllocator>(
            "X", DoubleValue(90.0),
            "Y", DoubleValue(80.0),
            "Rho", StringValue("ns3::UniformRandomVariable[Min=0|Max=60]"))
        )
    );
    staMobility.Install(staNodes);

    // UDP SERVER APP on server
    uint16_t udpPort = 8000;
    UdpServerHelper udpServer(udpPort);
    ApplicationContainer serverApps = udpServer.Install(serverNode);
    serverApps.Start(Seconds(0.5));
    serverApps.Stop(Seconds(20.0));

    // UDP CLIENTS on STAs (each STA sends to server)
    for (uint32_t i = 0; i < nStas; ++i)
    {
        UdpClientHelper udpClient(csmaIfaces.GetAddress(0), udpPort);
        udpClient.SetAttribute("MaxPackets", UintegerValue(32000));
        udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
        udpClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps = udpClient.Install(staNodes.Get(i));
        clientApps.Start(Seconds(1.0 + i * 0.2)); // Staggered starts
        clientApps.Stop(Seconds(20.0));
    }

    // Enable NetAnim
    AnimationInterface anim("wifi-multi-ap-sta.xml");

    // Node colors: APs: yellow, STAs: blue, Server: red
    for (uint32_t i = 0; i < apNodes.GetN(); ++i)
    {
        anim.UpdateNodeColor(apNodes.Get(i), 255, 255, 0); // yellow
        anim.UpdateNodeDescription(apNodes.Get(i), "AP" + std::to_string(i + 1));
    }
    for (uint32_t i = 0; i < staNodes.GetN(); ++i)
    {
        anim.UpdateNodeColor(staNodes.Get(i), 0, 0, 255); // blue
        anim.UpdateNodeDescription(staNodes.Get(i), "STA" + std::to_string(i + 1));
    }
    anim.UpdateNodeColor(serverNode.Get(0), 255, 0, 0);
    anim.UpdateNodeDescription(serverNode.Get(0), "Server");

    // PHY Traces for NetAnim
    phy.EnablePcapAll("wifi-multi-ap-sta", false);

    // Start simulation
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}