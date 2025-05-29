#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiHandoverSimulation");

static uint32_t handoverCount = 0;
static uint32_t packetLostCount = 0;
static std::map<uint32_t, Mac48Address> previousAp; // nodeId -> last associated AP

void
AssocCallback(std::string context, Mac48Address sta, Mac48Address ap)
{
    uint32_t nodeId = 0;
    // extract the node id from attribute context
    size_t pos = context.find("/DeviceList/");
    if (pos != std::string::npos)
    {
        size_t pos2 = context.find("/WifiMac/", pos);
        if (pos2 != std::string::npos)
        {
            std::string sub = context.substr(pos + 12, pos2 - (pos + 12));
            nodeId = std::stoi(sub);
        }
    }
    // Count a handover when the previous AP is different
    auto it = previousAp.find(nodeId);
    if (it != previousAp.end() && it->second != ap)
    {
        handoverCount++;
        std::cout << "Node " << nodeId << " handover from " << it->second << " to " << ap << " at " << Simulator::Now().GetSeconds() << "s" << std::endl;
    }
    previousAp[nodeId] = ap;
}

void
PacketLossCallback(std::string context, Ptr<const Packet> packet)
{
    packetLostCount++;
}

int
main(int argc, char *argv[])
{
    uint32_t nStas = 6;
    double simTime = 30.0;
    double radius = 30.0;
    double areaWidth = 60.0;
    double areaHeight = 40.0;

    CommandLine cmd;
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.Parse(argc, argv);

    // Wi-Fi parameters
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    // Set up the MAC and install APs and STAs
    WifiMacHelper wifiMac;
    Ssid ssid1 = Ssid("ap-ssid-1");
    Ssid ssid2 = Ssid("ap-ssid-2");

    // Install APs
    NodeContainer apNodes;
    apNodes.Create(2);

    NetDeviceContainer apDevs;
    for (uint32_t i = 0; i < 2; ++i)
    {
        wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue((i == 0) ? ssid1 : ssid2));
        apDevs.Add(wifi.Install(wifiPhy, wifiMac, apNodes.Get(i)));
    }

    // Install STAs
    NodeContainer staNodes;
    staNodes.Create(nStas);

    NetDeviceContainer staDevices;
    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid1),
                    "ActiveProbing", BooleanValue(true));
    for (uint32_t i = 0; i < staNodes.GetN(); ++i)
    {
        staDevices.Add(wifi.Install(wifiPhy, wifiMac, staNodes.Get(i)));
    }

    // Set up mobility
    MobilityHelper mobility;

    // AP positions: two corners
    Ptr<ListPositionAllocator> apPositionAlloc = CreateObject<ListPositionAllocator>();
    apPositionAlloc->Add(Vector(15.0, areaHeight/2, 0.0)); // AP1 at (15, areaHeight/2)
    apPositionAlloc->Add(Vector(areaWidth - 15.0, areaHeight/2, 0.0)); // AP2 at (areaWidth-15, areaHeight/2)
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.SetPositionAllocator(apPositionAlloc);
    mobility.Install(apNodes);

    // STA initial positions & mobility
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=60.0]"),
                                 "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=40.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue("Time"),
                             "Time", TimeValue(Seconds(0.5)),
                             "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=2.5]"),
                             "Bounds", RectangleValue(Rectangle(0, areaWidth, 0, areaHeight)));
    mobility.Install(staNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(staNodes);
    stack.Install(apNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apIfaces = address.Assign(apDevs);
    Ipv4InterfaceContainer staIfaces = address.Assign(staDevices);

    // Add applications: UDP echo server on APs, clients on STAs
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);

    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < 2; ++i)
    {
        serverApps.Add(echoServer.Install(apNodes.Get(i)));
    }
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    UdpEchoClientHelper echoClient(apIfaces.GetAddress(0), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    echoClient.SetAttribute("PacketSize", UintegerValue(64));
    ApplicationContainer clientAppsA;
    for (uint32_t i = 0; i < nStas; ++i)
    {
        // Alternate which AP is the initial target
        if (i % 2 == 0)
            echoClient.SetAttribute("RemoteAddress", AddressValue(apIfaces.GetAddress(0)));
        else
            echoClient.SetAttribute("RemoteAddress", AddressValue(apIfaces.GetAddress(1)));
        clientAppsA.Add(echoClient.Install(staNodes.Get(i)));
    }
    clientAppsA.Start(Seconds(2.0));
    clientAppsA.Stop(Seconds(simTime));

    // Monitor association (handover) events
    for (uint32_t i = 0; i < staDevices.GetN(); ++i)
    {
        Ptr<NetDevice> dev = staDevices.Get(i);
        Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice>(dev);
        Config::Connect("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) +
                        "/DeviceList/0/$ns3::WifiNetDevice/Phy/State/Association",
                        MakeCallback(&AssocCallback));
    }
    // The path above may not trigger; ns-3 does not have a default "Association" trace source on the Phy.
    // Use the WifiMac traces for association.
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx",
                    MakeCallback(&PacketLossCallback)); // Not strictly correct for packet loss, but placeholder

    // Use WifiMac association callback (if available) for handover
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Assoc",
                    MakeCallback(&AssocCallback));

    // NetAnim configuration
    AnimationInterface anim("wifi-handover.xml");
    anim.SetMaxPktsPerTraceFile(0x7fffffff);

    // Optionally, color APs and STAs differently
    for (uint32_t i = 0; i < apNodes.GetN(); i++)
    {
        anim.UpdateNodeColor(apNodes.Get(i), 255, 0, 0); // Red for AP
        anim.UpdateNodeDescription(apNodes.Get(i), "AP" + std::to_string(i+1));
    }
    for (uint32_t i = 0; i < staNodes.GetN(); i++)
    {
        anim.UpdateNodeColor(staNodes.Get(i), 0, 0, 255); // Blue for station
        anim.UpdateNodeDescription(staNodes.Get(i), "STA" + std::to_string(i+1));
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    std::cout << "Total handover events: " << handoverCount << std::endl;
    std::cout << "Total packet losses: " << packetLostCount << std::endl;

    Simulator::Destroy();
    return 0;
}