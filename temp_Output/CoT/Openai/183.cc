#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging if needed
    // LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

    Time::SetResolution(Time::NS);

    // Parameters
    uint32_t numAps = 2;
    uint32_t numStas = 4;
    double simTime = 10.0;

    NodeContainer apNodes;
    apNodes.Create(numAps);

    NodeContainer staNodes;
    staNodes.Create(numStas);

    // Wifi setup
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    WifiMacHelper mac;
    Ssid ssids[2];
    ssids[0] = Ssid("ap-ssid-0");
    ssids[1] = Ssid("ap-ssid-1");

    // Create AP devices
    NetDeviceContainer apDevices;
    for (uint32_t i = 0; i < numAps; ++i)
    {
        mac.SetType("ns3::ApWifiMac",
            "Ssid", SsidValue(ssids[i])
        );
        NetDeviceContainer apDevice = wifi.Install(phy, mac, apNodes.Get(i));
        apDevices.Add(apDevice);
    }

    // Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> apPositionAlloc = CreateObject<ListPositionAllocator>();
    apPositionAlloc->Add(Vector(20.0, 50.0, 0.0));
    apPositionAlloc->Add(Vector(80.0, 50.0, 0.0));
    mobility.SetPositionAllocator(apPositionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);

    Ptr<ListPositionAllocator> staPositionAlloc = CreateObject<ListPositionAllocator>();
    staPositionAlloc->Add(Vector(10.0, 48.0, 0.0)); // STA0 close to AP0
    staPositionAlloc->Add(Vector(30.0, 52.0, 0.0)); // STA1 close to AP0
    staPositionAlloc->Add(Vector(70.0, 49.0, 0.0)); // STA2 close to AP1
    staPositionAlloc->Add(Vector(90.0, 54.0, 0.0)); // STA3 close to AP1

    mobility.SetPositionAllocator(staPositionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNodes);

    // STA devices and associations
    NetDeviceContainer staDevices;
    for (uint32_t i = 0; i < numStas; ++i)
    {
        uint32_t apIndex = (i < 2) ? 0 : 1; // First two STAs to AP0, next two to AP1
        mac.SetType("ns3::StaWifiMac",
            "Ssid", SsidValue(ssids[apIndex])
        );
        NetDeviceContainer staDevice = wifi.Install(phy, mac, staNodes.Get(i));
        staDevices.Add(staDevice);
    }

    // Internet stack
    NodeContainer allNodes;
    allNodes.Add(apNodes);
    allNodes.Add(staNodes);

    InternetStackHelper stack;
    stack.Install(allNodes);

    // Assign IPs
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> staIfs(numStas);
    Ipv4InterfaceContainer apIfs;
    address.SetBase("10.1.1.0", "255.255.255.0");
    NetDeviceContainer fullStaAp;
    fullStaAp.Add(staDevices);
    fullStaAp.Add(apDevices);
    Ipv4InterfaceContainer interfaces = address.Assign(fullStaAp);

    // First numStas: STA IPs, next numAps: AP IPs
    std::vector<Ipv4Address> staAddresses;
    std::vector<Ipv4Address> apAddresses;
    for (uint32_t i = 0; i < numStas; ++i)
        staAddresses.push_back(interfaces.GetAddress(i));
    for (uint32_t i = 0; i < numAps; ++i)
        apAddresses.push_back(interfaces.GetAddress(numStas + i));

    // UDP Traffic: each STA sends to the STA associated with another AP
    uint16_t udpPort = 8000;
    ApplicationContainer serverApps, clientApps;
    for (uint32_t i = 0; i < numStas; ++i)
    {
        uint32_t target;
        if (i < 2)
            target = 2 + i; // STA0->STA2, STA1->STA3
        else
            target = i - 2; // STA2->STA0, STA3->STA1

        UdpServerHelper server(udpPort + i);
        serverApps.Add(server.Install(staNodes.Get(i)));
    }

    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    for (uint32_t i = 0; i < numStas; ++i)
    {
        uint32_t target;
        if (i < 2)
            target = 2 + i; // STA0->STA2, STA1->STA3
        else
            target = i - 2; // STA2->STA0, STA3->STA1

        UdpClientHelper client(staAddresses[target], udpPort + target);
        client.SetAttribute("MaxPackets", UintegerValue(100));
        client.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
        client.SetAttribute("PacketSize", UintegerValue(1024));
        clientApps.Add(client.Install(staNodes.Get(i)));
    }

    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime - 1));

    // Enable NetAnim
    AnimationInterface anim("wifi-ap-sta-netanim.xml");
    // Set node descriptions and colors
    for (uint32_t i = 0; i < apNodes.GetN(); ++i)
    {
        anim.UpdateNodeDescription(apNodes.Get(i), "AP" + std::to_string(i));
        anim.UpdateNodeColor(apNodes.Get(i), 0, 255, 0); // Green
    }
    for (uint32_t i = 0; i < staNodes.GetN(); ++i)
    {
        anim.UpdateNodeDescription(staNodes.Get(i), "STA" + std::to_string(i));
        anim.UpdateNodeColor(staNodes.Get(i), 0, 0, 255); // Blue
    }

    anim.EnablePacketMetadata(true);

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}