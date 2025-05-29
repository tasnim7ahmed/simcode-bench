#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t nAps = 2;
    uint32_t nStas = 4;
    double simTime = 10.0;

    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer apNodes, staNodes;
    apNodes.Create(nAps);
    staNodes.Create(nStas);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac);

    WifiMacHelper mac;
    Ssid ssid1 = Ssid("network-1");
    Ssid ssid2 = Ssid("network-2");

    NetDeviceContainer apDevices, staDevices;

    // Assign stations to nearest AP
    // First half to AP1, second half to AP2
    NodeContainer staGroup1, staGroup2;
    for (uint32_t i = 0; i < nStas; ++i) {
        if (i < nStas / 2)
            staGroup1.Add(staNodes.Get(i));
        else
            staGroup2.Add(staNodes.Get(i));
    }

    // Install on group 1 (SSID 1)
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid1),
                "ActiveProbing", BooleanValue(false));
    staDevices.Add(wifi.Install(phy, mac, staGroup1));

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid2),
                "ActiveProbing", BooleanValue(false));
    staDevices.Add(wifi.Install(phy, mac, staGroup2));

    // AP 1
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid1));
    apDevices.Add(wifi.Install(phy, mac, apNodes.Get(0)));

    // AP 2
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid2));
    apDevices.Add(wifi.Install(phy, mac, apNodes.Get(1)));

    MobilityHelper mobility;
    // APs at fixed positions
    Ptr<ListPositionAllocator> apPos = CreateObject<ListPositionAllocator>();
    apPos->Add(Vector(0.0, 0.0, 0.0));
    apPos->Add(Vector(50.0, 0.0, 0.0));
    mobility.SetPositionAllocator(apPos);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);

    // STAs near their respective APs
    Ptr<ListPositionAllocator> staPos = CreateObject<ListPositionAllocator>();
    staPos->Add(Vector(-5.0, 5.0, 0.0));
    staPos->Add(Vector(5.0, -5.0, 0.0));
    staPos->Add(Vector(45.0, 5.0, 0.0));
    staPos->Add(Vector(55.0, -5.0, 0.0));
    mobility.SetPositionAllocator(staPos);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNodes);

    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apIfs = address.Assign(apDevices);
    Ipv4InterfaceContainer staIfs = address.Assign(staDevices);

    // UDP Traffic: Each STA sends to its AP, each AP sends to one STA
    uint16_t port = 50000;
    ApplicationContainer serverApps, clientApps;

    // STA -> AP (each STA sends to its AP)
    for (uint32_t i = 0; i < nStas; ++i) {
        Ptr<Node> sta = staNodes.Get(i);
        uint32_t apIdx = (i < nStas / 2) ? 0 : 1;
        Ipv4Address apAddr = apIfs.GetAddress(apIdx);

        UdpServerHelper server(port + i);
        serverApps.Add(server.Install(apNodes.Get(apIdx)));

        UdpClientHelper client(apAddr, port + i);
        client.SetAttribute("MaxPackets", UintegerValue(320));
        client.SetAttribute("Interval", TimeValue(MilliSeconds(25)));
        client.SetAttribute("PacketSize", UintegerValue(1024));
        clientApps.Add(client.Install(sta));
    }

    // AP -> STA (each AP sends to one associated STA)
    for (uint32_t apIdx = 0; apIdx < 2; ++apIdx) {
        uint32_t staIdx = apIdx * 2; // first STA in each group
        Ipv4Address staAddr = staIfs.GetAddress(staIdx);

        UdpServerHelper server(port + 10 + apIdx);
        serverApps.Add(server.Install(staNodes.Get(staIdx)));

        UdpClientHelper client(staAddr, port + 10 + apIdx);
        client.SetAttribute("MaxPackets", UintegerValue(200));
        client.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
        client.SetAttribute("PacketSize", UintegerValue(1024));
        clientApps.Add(client.Install(apNodes.Get(apIdx)));
    }

    serverApps.Start(Seconds(1.0));
    clientApps.Start(Seconds(1.2));
    serverApps.Stop(Seconds(simTime));
    clientApps.Stop(Seconds(simTime));

    AnimationInterface anim("wifi-two-ap-four-sta.xml");
    for (uint32_t i = 0; i < nAps; ++i) {
        anim.UpdateNodeDescription(apNodes.Get(i), "AP" + std::to_string(i+1));
        anim.UpdateNodeColor(apNodes.Get(i), 255, 0, 0);
    }
    for (uint32_t i = 0; i < nStas; ++i) {
        anim.UpdateNodeDescription(staNodes.Get(i), "STA" + std::to_string(i+1));
        anim.UpdateNodeColor(staNodes.Get(i), 0, 255, 0);
    }
    anim.EnablePacketMetadata(true);
    anim.EnableIpv4RouteTracking("wifi-routes.xml", Seconds(0), Seconds(simTime), Seconds(0.25));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}