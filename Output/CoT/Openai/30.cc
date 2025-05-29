#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"
#include "ns3/config-store.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Wifi80211nTosSimulation");

uint64_t g_rxBytes = 0;

void RxCallback(Ptr<const Packet> packet, const Address &address)
{
    g_rxBytes += packet->GetSize();
}

int main(int argc, char *argv[])
{
    // Simulation parameters with default values
    uint32_t nSta = 4;
    uint32_t mcs = 0;
    uint32_t channelWidth = 20;
    bool shortGi = false;
    double distance = 5.0;
    bool useRtsCts = false;
    double simulationTime = 10.0;

    CommandLine cmd;
    cmd.AddValue ("nSta", "Number of Wi-Fi stations", nSta);
    cmd.AddValue ("mcs", "HT MCS value (0-7)", mcs);
    cmd.AddValue ("channelWidth", "Channel width (20 or 40 MHz)", channelWidth);
    cmd.AddValue ("shortGi", "Use short guard interval (true/false)", shortGi);
    cmd.AddValue ("distance", "Distance between STA and AP (meters)", distance);
    cmd.AddValue ("useRtsCts", "Enable RTS/CTS (true/false)", useRtsCts);
    cmd.AddValue ("simulationTime", "Simulation time (s)", simulationTime);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nSta);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Configure Wi-Fi channel and PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());
    phy.Set ("ChannelWidth", UintegerValue(channelWidth));
    phy.Set ("ShortGuardEnabled", BooleanValue(shortGi));
    phy.Set ("RxSensitivity", DoubleValue(-82.0)); // Standard default

    // Configure Wi-Fi with 802.11n HT
    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211n_2_4GHZ);
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HtMcs" + std::to_string(mcs)),
                                 "ControlMode", StringValue("HtMcs0"),
                                 "RtsCtsThreshold", UintegerValue (useRtsCts ? 0 : 2347));

    WifiMacHelper mac;
    Ssid ssid = Ssid ("ns3-80211n");
    // Station MAC
    mac.SetType ("ns3::StaWifiMac",
                 "Ssid", SsidValue (ssid),
                 "ActiveProbing", BooleanValue (false));
    NetDeviceContainer staDevices = wifi.Install (phy, mac, wifiStaNodes);
    // AP MAC
    mac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid));
    NetDeviceContainer apDevice = wifi.Install (phy, mac, wifiApNode);

    // Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    // AP at origin
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);

    // Stations uniformly spaced in a circle around AP
    Ptr<ListPositionAllocator> staPosAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < nSta; ++i) {
        double angle = 2*M_PI*i/nSta;
        staPosAlloc->Add(Vector(distance * std::cos(angle), distance * std::sin(angle), 0.0));
    }
    mobility.SetPositionAllocator(staPosAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install (wifiApNode);
    stack.Install (wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign (apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);

    // UDP Servers on AP, one per TOS(DSCP) value
    uint32_t nTos = 4;
    std::vector<uint8_t> tosValues = {0x00, 0x20, 0x40, 0xb8}; // Default DSCP: BE, CS1, CS2, EF
    // For each station, generate one UDP flow to AP with different TOS
    ApplicationContainer serverApps;
    ApplicationContainer clientApps;
    uint16_t port = 4000;
    double appStart = 1.0;
    double appStop = simulationTime;

    for (uint32_t i = 0; i < nSta; ++i)
    {
        for (uint32_t t = 0; t < nTos; ++t)
        {
            UdpServerHelper server(port + i * nTos + t);
            ApplicationContainer serverApp = server.Install(wifiApNode.Get(0));
            serverApp.Start(Seconds(appStart));
            serverApp.Stop(Seconds(appStop));
            serverApps.Add(serverApp);

            UdpClientHelper client(apInterface.GetAddress(0), port + i * nTos + t);
            client.SetAttribute("MaxPackets", UintegerValue(4294967295u));
            client.SetAttribute("Interval", TimeValue(MicroSeconds(100))); // 10,000 pps default
            client.SetAttribute("PacketSize", UintegerValue(1472));
            client.SetAttribute("TypeOfService", UintegerValue(tosValues[t]));
            ApplicationContainer clientApp = client.Install(wifiStaNodes.Get(i));
            clientApp.Start(Seconds(appStart));
            clientApp.Stop(Seconds(appStop));
            clientApps.Add(clientApp);
        }
    }

    // Trace received packets on server side for throughput
    Config::ConnectWithoutContext(
        "/NodeList/0/ApplicationList/*/$ns3::UdpServer/Rx", MakeCallback(&RxCallback));

    Simulator::Stop(Seconds(simulationTime + 1.0));
    Simulator::Run();

    double throughputMbps = (g_rxBytes * 8.0) / (simulationTime - appStart) / 1e6;

    std::cout << "====== Simulation Results ======" << std::endl;
    std::cout << "Stations: " << nSta << std::endl;
    std::cout << "HT MCS: " << mcs << std::endl;
    std::cout << "Channel width: " << channelWidth << " MHz" << std::endl;
    std::cout << "Guard interval: " << (shortGi ? "Short" : "Long") << std::endl;
    std::cout << "Distance to AP: " << distance << " m" << std::endl;
    std::cout << "RTS/CTS: " << (useRtsCts ? "Enabled" : "Disabled") << std::endl;
    std::cout << "Aggregated UDP throughput: " << throughputMbps << " Mbps" << std::endl;

    Simulator::Destroy();
    return 0;
}