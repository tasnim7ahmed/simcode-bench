#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    uint32_t nAps = 3;
    uint32_t nStas = 6;
    uint32_t nServer = 1;
    double simTime = 30.0;

    NodeContainer wifiAps;
    wifiAps.Create(nAps);

    NodeContainer wifiStas;
    wifiStas.Create(nStas);

    NodeContainer serverNode;
    serverNode.Create(nServer);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    WifiMacHelper mac;
    Ssid ssid[3];
    for (uint32_t i = 0; i < nAps; ++i)
    {
        std::ostringstream ss;
        ss << "ssid-ap" << i+1;
        ssid[i] = Ssid(ss.str());
    }

    NetDeviceContainer staDevices[3];
    NetDeviceContainer apDevices;

    // Distribute 2 stations to each AP
    for (uint32_t i = 0; i < nAps; ++i)
    {
        mac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid[i]),
                    "ActiveProbing", BooleanValue(false));
        NodeContainer staGroup;
        for (uint32_t j = 0; j < nStas/nAps; ++j)
        {
            staGroup.Add(wifiStas.Get(i * (nStas/nAps) + j));
        }
        staDevices[i] = wifi.Install(phy, mac, staGroup);

        mac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid[i]));
        apDevices.Add(wifi.Install(phy, mac, wifiAps.Get(i)));
    }

    // Use point-to-point to server
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));
    NodeContainer apToServer;
    apToServer.Add(wifiAps);
    apToServer.Add(serverNode);

    NetDeviceContainer p2pDevices;
    for (uint32_t i = 0; i < nAps; ++i)
    {
        NodeContainer linkNodes;
        linkNodes.Add(wifiAps.Get(i));
        linkNodes.Add(serverNode.Get(0));
        NetDeviceContainer linkDevs = p2p.Install(linkNodes);
        p2pDevices.Add(linkDevs);
    }

    InternetStackHelper stack;
    for (uint32_t i = 0; i < nAps; ++i)
    {
        for (uint32_t j = 0; j < nStas/nAps; ++j)
            stack.Install(wifiStas.Get(i * (nStas/nAps) + j));
        stack.Install(wifiAps.Get(i));
    }
    stack.Install(serverNode);

    Ipv4AddressHelper address;

    std::vector<Ipv4InterfaceContainer> staIfs(nAps);
    std::vector<Ipv4InterfaceContainer> apIfs(nAps);
    Ipv4InterfaceContainer serverIfs;

    // Assign IPs to WiFi networks for each AP group
    for (uint32_t i = 0; i < nAps; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i+1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        NodeContainer staGroup;
        for (uint32_t j = 0; j < nStas/nAps; ++j)
            staGroup.Add(wifiStas.Get(i * (nStas/nAps) + j));
        NetDeviceContainer ndc;
        ndc.Add(staDevices[i]);
        ndc.Add(apDevices.Get(i));
        Ipv4InterfaceContainer ifcont = address.Assign(ndc);
        for (uint32_t j = 0; j < nStas/nAps; ++j)
            staIfs[i].Add(ifcont.Get(j));
        apIfs[i].Add(ifcont.Get(staDevices[i].GetN()));
    }

    // Assign IPs to point-to-point links
    for (uint32_t i = 0; i < nAps; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.2." << i+1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        NetDeviceContainer p2plink;
        p2plink.Add(p2pDevices.Get(i*2));
        p2plink.Add(p2pDevices.Get(i*2+1));
        Ipv4InterfaceContainer ifcont = address.Assign(p2plink);
        apIfs[i].Add(ifcont.Get(0));
        serverIfs.Add(ifcont.Get(1));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Mobility
    MobilityHelper mobilityAp;
    Ptr<ListPositionAllocator> apAlloc = CreateObject<ListPositionAllocator>();
    apAlloc->Add(Vector(30.0, 50.0, 0.0));
    apAlloc->Add(Vector(70.0, 50.0, 0.0));
    apAlloc->Add(Vector(110.0, 50.0, 0.0));
    mobilityAp.SetPositionAllocator(apAlloc);
    mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityAp.Install(wifiAps);

    MobilityHelper mobilitySta;
    mobilitySta.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                                "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=2.0]"),
                                "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
                                "PositionAllocator", PointerValue(CreateObjectWithAttributes<RandomRectanglePositionAllocator>(
                                        "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=140.0]"),
                                        "Y", StringValue("ns3::UniformRandomVariable[Min=40.0|Max=60.0]"))));
    mobilitySta.Install(wifiStas);

    MobilityHelper mobilityServer;
    mobilityServer.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityServer.Install(serverNode);

    // UDP Applications: install UDP server on remote serverNode(0)
    uint16_t port = 9000;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(serverNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simTime));

    // Each STA sends UDP to the server
    for (uint32_t i = 0; i < nStas; ++i)
    {
        UdpClientHelper client(serverIfs.GetAddress(0), port);
        client.SetAttribute("MaxPackets", UintegerValue(10000));
        client.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
        client.SetAttribute("PacketSize", UintegerValue(512));
        ApplicationContainer staApp = client.Install(wifiStas.Get(i));
        staApp.Start(Seconds(2.0 + i * 0.5));
        staApp.Stop(Seconds(simTime));
    }

    // NetAnim visualization
    AnimationInterface anim("wifi-multiap-netanim.xml");
    for (uint32_t i = 0; i < wifiAps.GetN(); ++i)
        anim.UpdateNodeDescription(wifiAps.Get(i), "AP" + std::to_string(i+1));
    for (uint32_t i = 0; i < wifiStas.GetN(); ++i)
        anim.UpdateNodeDescription(wifiStas.Get(i), "STA" + std::to_string(i+1));
    anim.UpdateNodeDescription(serverNode.Get(0), "Server");

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}