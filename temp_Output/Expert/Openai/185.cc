#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ssid.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t numAps = 3;
    uint32_t numStas = 6;
    double simulationTime = 20.0; // seconds
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));

    NodeContainer apNodes;
    apNodes.Create(numAps);

    NodeContainer staNodes;
    staNodes.Create(numStas);

    NodeContainer serverNode;
    serverNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("VhtMcs9"),
                                 "ControlMode", StringValue("VhtMcs0"));

    WifiMacHelper mac;
    std::vector<Ssid> ssids;
    for (uint32_t i = 0; i < numAps; ++i)
    {
        std::ostringstream ss;
        ss << "AP" << (i+1) << "-SSID";
        ssids.push_back(Ssid(ss.str()));
    }

    NetDeviceContainer apDevices;
    NetDeviceContainer staDevices;

    for (uint32_t i = 0; i < numAps; ++i)
    {
        // Install AP devices
        mac.SetType("ns3::ApWifiMac",
            "Ssid", SsidValue(ssids[i]));
        NetDeviceContainer apDev = wifi.Install(phy, mac, apNodes.Get(i));
        apDevices.Add(apDev);
    }

    // Divide stations between APs
    for (uint32_t i = 0; i < numStas; ++i)
    {
        uint32_t apIndex = i / 2; // 0-1 to AP0, 2-3 to AP1, 4-5 to AP2
        mac.SetType("ns3::StaWifiMac",
            "Ssid", SsidValue(ssids[apIndex]));
        NetDeviceContainer staDev = wifi.Install(phy, mac, staNodes.Get(i));
        staDevices.Add(staDev);
    }

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(serverNode);
    stack.Install(apNodes);
    stack.Install(staNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.255.0");
    Ipv4InterfaceContainer apIfs = address.Assign(apDevices);
    Ipv4InterfaceContainer staIfs = address.Assign(staDevices);

    // Point-to-point between an AP and the server
    NodeContainer p2pNodes;
    p2pNodes.Add(apNodes.Get(0));
    p2pNodes.Add(serverNode.Get(0));
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer p2pDevices = p2p.Install(p2pNodes);

    address.SetBase("10.2.0.0", "255.255.255.0");
    Ipv4InterfaceContainer serverIfs = address.Assign(p2pDevices);

    // Mobility for APs (static)
    MobilityHelper mobilityAp;
    Ptr<ListPositionAllocator> apPos = CreateObject<ListPositionAllocator>();
    apPos->Add(Vector(0.0, 0.0, 0.0));
    apPos->Add(Vector(50.0, 0.0, 0.0));
    apPos->Add(Vector(100.0, 0.0, 0.0));
    mobilityAp.SetPositionAllocator(apPos);
    mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityAp.Install(apNodes);

    // Mobility for stations
    MobilityHelper mobilitySta;
    mobilitySta.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                     "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                     "Y", StringValue("ns3::UniformRandomVariable[Min=10.0|Max=30.0]"));
    mobilitySta.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                                 "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                                 "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
                                 "PositionAllocator", PointerValue(mobilitySta.GetPositionAllocator()));
    mobilitySta.Install(staNodes);

    // Server node static
    MobilityHelper mobilityServer;
    mobilityServer.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityServer.SetPositionAllocator("ns3::ListPositionAllocator",
                                        "PositionList", VectorValue(MakeVector(Vector(150.0, 0.0, 0.0))));
    mobilityServer.Install(serverNode);

    // Install Applications: UDP EchoServer on server, clients on stations
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApp = echoServer.Install(serverNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simulationTime));

    for (uint32_t i = 0; i < numStas; ++i)
    {
        UdpEchoClientHelper echoClient(serverIfs.GetAddress(1), echoPort);
        echoClient.SetAttribute("MaxPackets", UintegerValue(50));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.4)));
        echoClient.SetAttribute("PacketSize", UintegerValue(256));
        ApplicationContainer clientApp = echoClient.Install(staNodes.Get(i));
        clientApp.Start(Seconds(2.0 + i * 0.2));
        clientApp.Stop(Seconds(simulationTime));
    }

    // NetAnim
    AnimationInterface anim("wifi-mobility-netanim.xml");
    for (uint32_t i = 0; i < apNodes.GetN(); ++i)
        anim.UpdateNodeDescription(apNodes.Get(i), "AP" + std::to_string(i+1));
    for (uint32_t i = 0; i < staNodes.GetN(); ++i)
        anim.UpdateNodeDescription(staNodes.Get(i), "STA" + std::to_string(i+1));
    anim.UpdateNodeDescription(serverNode.Get(0), "Server");
    anim.SetMaxPktsPerTraceFile(50000);

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}