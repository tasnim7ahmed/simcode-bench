#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"

using namespace ns3;

struct WifiStandardBand
{
    WifiStandard standard;
    WifiPhyBand band;
};

// Helper function to map version string to Wi-Fi standard and band
WifiStandardBand
ParseWifiVersion(const std::string& version)
{
    if (version == "80211a")
        return { WIFI_STANDARD_80211a, WIFI_PHY_BAND_5GHZ };
    if (version == "80211b")
        return { WIFI_STANDARD_80211b, WIFI_PHY_BAND_2_4GHZ };
    if (version == "80211g")
        return { WIFI_STANDARD_80211g, WIFI_PHY_BAND_2_4GHZ };
    if (version == "80211n2_4")
        return { WIFI_STANDARD_80211n, WIFI_PHY_BAND_2_4GHZ };
    if (version == "80211n5")
        return { WIFI_STANDARD_80211n, WIFI_PHY_BAND_5GHZ };
    if (version == "80211ac")
        return { WIFI_STANDARD_80211ac, WIFI_PHY_BAND_5GHZ };
    if (version == "80211ax2_4")
        return { WIFI_STANDARD_80211ax, WIFI_PHY_BAND_2_4GHZ };
    if (version == "80211ax5")
        return { WIFI_STANDARD_80211ax, WIFI_PHY_BAND_5GHZ };
    // Default to 802.11a
    return { WIFI_STANDARD_80211a, WIFI_PHY_BAND_5GHZ };
}

// Throughput calculation helper
void
CalculateThroughput(Ptr<Application> app, double startTime, std::string tag)
{
    static uint64_t lastTotalRx = 0;
    static Time lastTime = Seconds(startTime);

    Ptr<PacketSink> sink = DynamicCast<PacketSink>(app);
    if (sink)
    {
        uint64_t curTotalRx = sink->GetTotalRx();
        Time curTime = Simulator::Now();
        double throughput = ((curTotalRx - lastTotalRx) * 8.0) / (curTime.GetSeconds() - lastTime.GetSeconds()) / 1e6; // Mbps

        std::cout << tag << " Throughput: " << throughput << " Mbps at " << curTime.GetSeconds() << "s" << std::endl;

        lastTotalRx = curTotalRx;
        lastTime = curTime;
        Simulator::Schedule(Seconds(1.0), &CalculateThroughput, app, startTime, tag);
    }
}

int
main(int argc, char *argv[])
{
    std::string apVersion = "80211ac";
    std::string staVersion = "80211a";
    std::string apRateManager = "ns3::AarfWifiManager";
    std::string staRateManager = "ns3::MinstrelHtWifiManager";
    bool trafficFromSta = true; // true: STA->AP, false: AP->STA
    double simTime = 10.0;
    uint32_t payloadSize = 1024;

    CommandLine cmd;
    cmd.AddValue("apVersion", "Wi-Fi version for AP (e.g., 80211ac)", apVersion);
    cmd.AddValue("staVersion", "Wi-Fi version for STA (e.g., 80211a)", staVersion);
    cmd.AddValue("apRateManager", "Rate manager for AP", apRateManager);
    cmd.AddValue("staRateManager", "Rate manager for STA", staRateManager);
    cmd.AddValue("trafficFromSta", "Generate traffic from STA to AP if true, else AP to STA", trafficFromSta);
    cmd.AddValue("simTime", "Simulation time (s)", simTime);
    cmd.Parse(argc, argv);

    NodeContainer wifiStaNode, wifiApNode;
    wifiStaNode.Create(1);
    wifiApNode.Create(1);

    Ssid ssid = Ssid("wifi-backward-compat");

    // Setup AP Wi-Fi PHY/MAC
    YansWifiChannelHelper channelAp = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phyAp = YansWifiPhyHelper::Default();
    phyAp.SetChannel(channelAp.Create());

    WifiHelper wifiAp;
    WifiMacHelper macAp;

    WifiStandardBand apStdBand = ParseWifiVersion(apVersion);
    wifiAp.SetStandard(apStdBand.standard);
    phyAp.Set("Frequency", UintegerValue(apStdBand.band == WIFI_PHY_BAND_2_4GHZ ? 2412 : 5180));

    wifiAp.SetRemoteStationManager(apRateManager);

    macAp.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice = wifiAp.Install(phyAp, macAp, wifiApNode);

    // Setup STA Wi-Fi PHY/MAC
    YansWifiChannelHelper channelSta = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phySta = YansWifiPhyHelper::Default();
    phySta.SetChannel(channelSta.Create());

    WifiHelper wifiSta;
    WifiMacHelper macSta;

    WifiStandardBand staStdBand = ParseWifiVersion(staVersion);
    wifiSta.SetStandard(staStdBand.standard);
    phySta.Set("Frequency", UintegerValue(staStdBand.band == WIFI_PHY_BAND_2_4GHZ ? 2412 : 5180));

    wifiSta.SetRemoteStationManager(staRateManager);

    macSta.SetType("ns3::StaWifiMac",
                   "Ssid", SsidValue(ssid),
                   "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevice = wifiSta.Install(phySta, macSta, wifiStaNode);

    // Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // AP
    positionAlloc->Add(Vector(3.0, 0.0, 0.0)); // STA
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNode);

    // Internet Stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");

    NetDeviceContainer allDevices;
    allDevices.Add(apDevice);
    allDevices.Add(staDevice);
    Ipv4InterfaceContainer interfaces = address.Assign(allDevices);

    // Applications
    uint16_t port = 9999;

    ApplicationContainer serverApp, clientApp;

    if (trafficFromSta)
    {
        // Server at AP, Client at STA
        UdpServerHelper udpServer(port);
        serverApp = udpServer.Install(wifiApNode.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(simTime));

        UdpClientHelper udpClient(interfaces.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000000));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        udpClient.SetAttribute("PacketSize", UintegerValue(payloadSize));
        clientApp = udpClient.Install(wifiStaNode.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(simTime));
    }
    else
    {
        // Server at STA, Client at AP
        UdpServerHelper udpServer(port);
        serverApp = udpServer.Install(wifiStaNode.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(simTime));

        UdpClientHelper udpClient(interfaces.GetAddress(1), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000000));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        udpClient.SetAttribute("PacketSize", UintegerValue(payloadSize));
        clientApp = udpClient.Install(wifiApNode.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(simTime));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Throughput calculation: print every second after 2s
    Ptr<Application> server = serverApp.Get(0);
    Simulator::Schedule(Seconds(2.0), &CalculateThroughput, server, 2.0,
                        trafficFromSta ? "STA->AP" : "AP->STA");

    Simulator::Stop(Seconds(simTime + 1));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}