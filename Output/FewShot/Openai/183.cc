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
    // Create nodes for STAs and APs
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(4);
    NodeContainer wifiApNodes;
    wifiApNodes.Create(2);

    // Configure the physical and MAC layers
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac);

    WifiMacHelper mac;

    // Setup SSIDs for two APs
    Ssid ssid1 = Ssid("ap-1-ssid");
    Ssid ssid2 = Ssid("ap-2-ssid");

    // Install devices for APs with separate SSIDs
    NetDeviceContainer apDevices1, staDevices1, apDevices2, staDevices2;

    // Assign first two STAs to AP1, last two to AP2 (nearest based on mobility)
    std::vector<Ptr<Node>> staNodes1 = {wifiStaNodes.Get(0), wifiStaNodes.Get(1)};
    std::vector<Ptr<Node>> staNodes2 = {wifiStaNodes.Get(2), wifiStaNodes.Get(3)};

    NodeContainer staNodes1Cont, staNodes2Cont;
    for (auto n : staNodes1) staNodes1Cont.Add(n);
    for (auto n : staNodes2) staNodes2Cont.Add(n);

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid1),
                "ActiveProbing", BooleanValue(false));
    staDevices1 = wifi.Install(phy, mac, staNodes1Cont);

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid2),
                "ActiveProbing", BooleanValue(false));
    staDevices2 = wifi.Install(phy, mac, staNodes2Cont);

    // AP1
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid1));
    apDevices1 = wifi.Install(phy, mac, wifiApNodes.Get(0));

    // AP2
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid2));
    apDevices2 = wifi.Install(phy, mac, wifiApNodes.Get(1));

    // Mobility
    MobilityHelper mobility;

    // Position APs
    Ptr<ListPositionAllocator> apPosAlloc = CreateObject<ListPositionAllocator>();
    apPosAlloc->Add(Vector(0.0, 0.0, 0.0));   // AP1 at (0, 0)
    apPosAlloc->Add(Vector(40.0, 0.0, 0.0));  // AP2 at (40, 0)
    mobility.SetPositionAllocator(apPosAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNodes);

    // Position STAs: close to respective APs for "nearest" association
    Ptr<ListPositionAllocator> staPosAlloc = CreateObject<ListPositionAllocator>();
    staPosAlloc->Add(Vector(5.0,  2.0, 0.0));   // STA0 (near AP1)
    staPosAlloc->Add(Vector(-2.0, -3.0, 0.0));  // STA1 (near AP1)
    staPosAlloc->Add(Vector(35.0,  1.0, 0.0));  // STA2 (near AP2)
    staPosAlloc->Add(Vector(45.0, -1.0, 0.0));  // STA3 (near AP2)
    mobility.SetPositionAllocator(staPosAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);

    // Install Internet stacks
    InternetStackHelper stack;
    stack.Install(wifiApNodes);
    stack.Install(wifiStaNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staIfs1 = address.Assign(staDevices1);
    Ipv4InterfaceContainer apIfs1  = address.Assign(apDevices1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer staIfs2 = address.Assign(staDevices2);
    Ipv4InterfaceContainer apIfs2  = address.Assign(apDevices2);

    // Applications: UDP Echo from all STAs to both APs (to show bidirectional traffic)
    uint16_t port = 9;
    UdpEchoServerHelper echoServer1(port);
    ApplicationContainer serverApps1 = echoServer1.Install(wifiApNodes.Get(0));
    serverApps1.Start(Seconds(1.0));
    serverApps1.Stop(Seconds(10.0));

    UdpEchoServerHelper echoServer2(port);
    ApplicationContainer serverApps2 = echoServer2.Install(wifiApNodes.Get(1));
    serverApps2.Start(Seconds(1.0));
    serverApps2.Stop(Seconds(10.0));

    // Each STA sends to its AP (nearest)
    UdpEchoClientHelper echoClient1(apIfs1.GetAddress(0), port);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(256));

    ApplicationContainer clientApps1a = echoClient1.Install(wifiStaNodes.Get(0));
    ApplicationContainer clientApps1b = echoClient1.Install(wifiStaNodes.Get(1));
    clientApps1a.Start(Seconds(2.0));
    clientApps1b.Start(Seconds(2.0));
    clientApps1a.Stop(Seconds(10.0));
    clientApps1b.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient2(apIfs2.GetAddress(0), port);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(256));

    ApplicationContainer clientApps2a = echoClient2.Install(wifiStaNodes.Get(2));
    ApplicationContainer clientApps2b = echoClient2.Install(wifiStaNodes.Get(3));
    clientApps2a.Start(Seconds(2.0));
    clientApps2b.Start(Seconds(2.0));
    clientApps2a.Stop(Seconds(10.0));
    clientApps2b.Stop(Seconds(10.0));

    // Enable NetAnim
    AnimationInterface anim("wifi-2ap-4sta.xml");

    // Set node descriptions
    anim.UpdateNodeDescription(wifiApNodes.Get(0), "AP1");
    anim.UpdateNodeDescription(wifiApNodes.Get(1), "AP2");
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {
        std::ostringstream oss;
        oss << "STA" << i;
        anim.UpdateNodeDescription(wifiStaNodes.Get(i), oss.str());
    }

    // Set node colors for clarity
    anim.UpdateNodeColor(wifiApNodes.Get(0), 255, 0, 0);      // AP1 Red
    anim.UpdateNodeColor(wifiApNodes.Get(1), 0, 0, 255);      // AP2 Blue
    anim.UpdateNodeColor(wifiStaNodes.Get(0), 0, 128, 0);     // STA0 Green
    anim.UpdateNodeColor(wifiStaNodes.Get(1), 0, 128, 0);     // STA1 Green
    anim.UpdateNodeColor(wifiStaNodes.Get(2), 255, 165, 0);   // STA2 Orange
    anim.UpdateNodeColor(wifiStaNodes.Get(3), 255, 165, 0);   // STA3 Orange

    // Enable packet metadata for NetAnim traffic flow animation
    Config::SetDefault("ns3::PacketMetadata::Enable", BooleanValue(true));

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}