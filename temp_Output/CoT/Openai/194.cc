#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTwoApStaUdpExample");

uint64_t g_bytesSent[8]; // Support up to 8 stations for example

void
TxTrace(uint32_t staIdx, Ptr<const Packet> packet)
{
    g_bytesSent[staIdx] += packet->GetSize();
}

int
main(int argc, char *argv[])
{
    uint32_t nStasPerAp = 2;
    double simulationTime = 10.0;
    uint32_t payloadSize = 1024;
    uint32_t dataRate = 1024 * 1024; // 1 Mbps
    uint16_t port = 50000;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nStasPerAp", "Number of stations per AP", nStasPerAp);
    cmd.AddValue("simulationTime", "Duration of simulation in seconds", simulationTime);
    cmd.AddValue("payloadSize", "UDP payload size (bytes)", payloadSize);
    cmd.AddValue("dataRate", "Application data rate (bps)", dataRate);
    cmd.Parse(argc, argv);

    uint32_t nStasTotal = nStasPerAp * 2;

    NodeContainer staNodes1, staNodes2, apNodes;
    staNodes1.Create(nStasPerAp);
    staNodes2.Create(nStasPerAp);
    apNodes.Create(2);

    // WiFi PHY/channel setup
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    WifiMacHelper mac;
    Ssid ssid1 = Ssid("ap-network-1");
    Ssid ssid2 = Ssid("ap-network-2");

    // Setup for group 1
    NetDeviceContainer staDevices1, apDevice1;
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid1),
                "ActiveProbing", BooleanValue(false));
    staDevices1 = wifi.Install(phy, mac, staNodes1);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid1));
    apDevice1 = wifi.Install(phy, mac, NodeContainer(apNodes.Get(0)));

    // Setup for group 2
    NetDeviceContainer staDevices2, apDevice2;
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid2),
                "ActiveProbing", BooleanValue(false));
    staDevices2 = wifi.Install(phy, mac, staNodes2);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid2));
    apDevice2 = wifi.Install(phy, mac, NodeContainer(apNodes.Get(1)));

    // Mobility Model
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    double distStep = 10.0;
    // AP 1 at (0,0), AP 2 at (50,0)
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(50.0, 0.0, 0.0));
    for (uint32_t i = 0; i < nStasPerAp; ++i)
        positionAlloc->Add(Vector(0.0 + (i+1) * distStep, 10.0, 0.0));
    for (uint32_t i = 0; i < nStasPerAp; ++i)
        positionAlloc->Add(Vector(50.0 + (i+1) * distStep, 10.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    NodeContainer allNodes;
    allNodes.Add(apNodes);
    allNodes.Add(staNodes1);
    allNodes.Add(staNodes2);
    mobility.Install(allNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer staIfs1, apIf1, staIfs2, apIf2;
    address.SetBase("10.1.1.0", "255.255.255.0");
    staIfs1 = address.Assign(staDevices1);
    apIf1 = address.Assign(apDevice1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    staIfs2 = address.Assign(staDevices2);
    apIf2 = address.Assign(apDevice2);

    // Routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Applications: Assign AP 1 as UDP server for all clients
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(apNodes.Get(0));
    serverApps.Start(Seconds(0.5));
    serverApps.Stop(Seconds(simulationTime));

    // UDP Clients (all STAs send CBR traffic to AP1 server)
    UdpClientHelper udpClient(apIf1.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(4294967295u));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(double(payloadSize * 8) / dataRate)));
    udpClient.SetAttribute("PacketSize", UintegerValue(payloadSize));

    ApplicationContainer clientApps;
    // Group 1
    for (uint32_t i = 0; i < nStasPerAp; ++i)
    {
        ApplicationContainer c = udpClient.Install(staNodes1.Get(i));
        c.Start(Seconds(1.0));
        c.Stop(Seconds(simulationTime));
        clientApps.Add(c);

        staDevices1.Get(i)->TraceConnectWithoutContext("PhyTxBegin",
            MakeBoundCallback(&TxTrace, i));
        g_bytesSent[i] = 0;
    }
    // Group 2 (offset index by nStasPerAp)
    for (uint32_t i = 0; i < nStasPerAp; ++i)
    {
        ApplicationContainer c = udpClient.Install(staNodes2.Get(i));
        c.Start(Seconds(1.0));
        c.Stop(Seconds(simulationTime));
        clientApps.Add(c);

        staDevices2.Get(i)->TraceConnectWithoutContext("PhyTxBegin",
            MakeBoundCallback(&TxTrace, i + nStasPerAp));
        g_bytesSent[i + nStasPerAp] = 0;
    }

    // Tracing
    phy.EnablePcapAll("wifi2apsta", true);
    AsciiTraceHelper ascii;
    phy.EnableAsciiAll(ascii.CreateFileStream("wifi2apsta.tr"));

    Simulator::Stop(Seconds(simulationTime + 0.5));
    Simulator::Run();

    // Print total data sent by each station
    std::cout << "Station data sent (bytes):\n";
    for (uint32_t i = 0; i < nStasTotal; ++i)
        std::cout << " STA" << i << ": " << g_bytesSent[i] << std::endl;

    Simulator::Destroy();
    return 0;
}