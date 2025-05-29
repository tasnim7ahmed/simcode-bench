#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    NodeContainer wifiApNodes;
    wifiApNodes.Create(2);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(4);

    // Place APs at (0,0,0) and (40,0,0)
    MobilityHelper mobilityAp;
    Ptr<ListPositionAllocator> apPositionAlloc = CreateObject<ListPositionAllocator>();
    apPositionAlloc->Add(Vector(0.0, 0.0, 0.0));
    apPositionAlloc->Add(Vector(40.0, 0.0, 0.0));
    mobilityAp.SetPositionAllocator(apPositionAlloc);
    mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityAp.Install(wifiApNodes);

    // Place STAs: 2 near AP1, 2 near AP2
    MobilityHelper mobilitySta;
    Ptr<ListPositionAllocator> staPositionAlloc = CreateObject<ListPositionAllocator>();
    staPositionAlloc->Add(Vector(5.0, 5.0, 0.0));    // STA0
    staPositionAlloc->Add(Vector(-5.0, -5.0, 0.0));  // STA1
    staPositionAlloc->Add(Vector(45.0, 5.0, 0.0));   // STA2
    staPositionAlloc->Add(Vector(35.0, -5.0, 0.0));  // STA3
    mobilitySta.SetPositionAllocator(staPositionAlloc);
    mobilitySta.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilitySta.Install(wifiStaNodes);

    // Install Wi-Fi PHY/MAC
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;

    // 2 SSIDs
    Ssid ssid1 = Ssid("ap-ssid-1");
    Ssid ssid2 = Ssid("ap-ssid-2");

    NetDeviceContainer staDevices1, apDevice1;
    NetDeviceContainer staDevices2, apDevice2;

    // STA0, STA1 connect to AP0/SSID1
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid1),
                "ActiveProbing", BooleanValue(false));
    staDevices1 = wifi.Install(phy, mac, NodeContainer(wifiStaNodes.Get(0), wifiStaNodes.Get(1)));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid1));
    apDevice1 = wifi.Install(phy, mac, wifiApNodes.Get(0));

    // STA2, STA3 connect to AP1/SSID2
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid2),
                "ActiveProbing", BooleanValue(false));
    staDevices2 = wifi.Install(phy, mac, NodeContainer(wifiStaNodes.Get(2), wifiStaNodes.Get(3)));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid2));
    apDevice2 = wifi.Install(phy, mac, wifiApNodes.Get(1));

    // Combine all net devices and nodes for ease of internet install
    NetDeviceContainer allStaDevices;
    allStaDevices.Add(staDevices1);
    allStaDevices.Add(staDevices2);

    NetDeviceContainer allApDevices;
    allApDevices.Add(apDevice1);
    allApDevices.Add(apDevice2);

    NodeContainer allNodes;
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
        allNodes.Add(wifiStaNodes.Get(i));
    for (uint32_t i = 0; i < wifiApNodes.GetN(); ++i)
        allNodes.Add(wifiApNodes.Get(i));

    // Install stack and assign IP addresses
    InternetStackHelper stack;
    stack.Install(allNodes);

    Ipv4AddressHelper address1;
    address1.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staIf1, apIf1;
    staIf1 = address1.Assign(staDevices1);
    apIf1 = address1.Assign(apDevice1);

    Ipv4AddressHelper address2;
    address2.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer staIf2, apIf2;
    staIf2 = address2.Assign(staDevices2);
    apIf2 = address2.Assign(apDevice2);

    // UDP Server on STA0 and STA2
    uint16_t port1 = 8000;
    UdpServerHelper server1(port1);
    ApplicationContainer serverApps1 = server1.Install(wifiStaNodes.Get(0));
    serverApps1.Start(Seconds(1.0));
    serverApps1.Stop(Seconds(10.0));

    uint16_t port2 = 8001;
    UdpServerHelper server2(port2);
    ApplicationContainer serverApps2 = server2.Install(wifiStaNodes.Get(2));
    serverApps2.Start(Seconds(1.0));
    serverApps2.Stop(Seconds(10.0));

    // UDP Client on STA3 to STA0
    UdpClientHelper client1(staIf1.GetAddress(0), port1);
    client1.SetAttribute("MaxPackets", UintegerValue(100));
    client1.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    client1.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps1 = client1.Install(wifiStaNodes.Get(3));
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(10.0));

    // UDP Client on STA1 to STA2
    UdpClientHelper client2(staIf2.GetAddress(0), port2);
    client2.SetAttribute("MaxPackets", UintegerValue(100));
    client2.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    client2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps2 = client2.Install(wifiStaNodes.Get(1));
    clientApps2.Start(Seconds(2.0));
    clientApps2.Stop(Seconds(10.0));

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // NetAnim
    AnimationInterface anim("wifi-ap-sta-netanim.xml");
    anim.SetConstantPosition(wifiApNodes.Get(0), 0, 0);
    anim.SetConstantPosition(wifiApNodes.Get(1), 40, 0);
    anim.SetConstantPosition(wifiStaNodes.Get(0), 5, 5);
    anim.SetConstantPosition(wifiStaNodes.Get(1), -5, -5);
    anim.SetConstantPosition(wifiStaNodes.Get(2), 45, 5);
    anim.SetConstantPosition(wifiStaNodes.Get(3), 35, -5);

    anim.UpdateNodeDescription(wifiApNodes.Get(0), "AP0");
    anim.UpdateNodeDescription(wifiApNodes.Get(1), "AP1");
    anim.UpdateNodeDescription(wifiStaNodes.Get(0), "STA0");
    anim.UpdateNodeDescription(wifiStaNodes.Get(1), "STA1");
    anim.UpdateNodeDescription(wifiStaNodes.Get(2), "STA2");
    anim.UpdateNodeDescription(wifiStaNodes.Get(3), "STA3");

    anim.UpdateNodeColor(wifiApNodes.Get(0), 255, 0, 0);
    anim.UpdateNodeColor(wifiApNodes.Get(1), 255, 0, 0);
    anim.UpdateNodeColor(wifiStaNodes.Get(0), 0, 255, 0);
    anim.UpdateNodeColor(wifiStaNodes.Get(1), 0, 255, 0);
    anim.UpdateNodeColor(wifiStaNodes.Get(2), 0, 0, 255);
    anim.UpdateNodeColor(wifiStaNodes.Get(3), 0, 0, 255);

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}