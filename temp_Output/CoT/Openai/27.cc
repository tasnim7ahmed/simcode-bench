#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wifi80211nGoodputSimulation");

double g_goodput = 0.0;

void
CalculateGoodput(Ptr<Application> app, Time start, Time stop)
{
    Ptr<BulkSendApplication> bulkApp = DynamicCast<BulkSendApplication>(app);
    Ptr<OnOffApplication> onoffApp = DynamicCast<OnOffApplication>(app);
    uint64_t totalRx = 0;
    if (bulkApp)
    {
        totalRx = bulkApp->GetTotalBytes();
    }
    else if (onoffApp)
    {
        totalRx = onoffApp->GetTotalBytes();
    }
    g_goodput = (double)totalRx * 8.0 / (stop.GetSeconds() - start.GetSeconds()) / 1e6;
    std::cout << "Goodput: " << g_goodput << " Mbps" << std::endl;
}

int main(int argc, char *argv[])
{
    // Default parameters
    uint32_t mcs = 7;
    uint32_t channelWidth = 40;
    bool shortGuardInterval = true;
    double simulationTime = 10.0;
    bool useTcp = false;
    bool enableRtsCts = false;
    double distance = 10.0;
    double expectedThroughput = 100.0;

    CommandLine cmd;
    cmd.AddValue("mcs", "Modulation and Coding Scheme (0-7)", mcs);
    cmd.AddValue("channelWidth", "Channel width in MHz (20|40)", channelWidth);
    cmd.AddValue("shortGuardInterval", "Short guard interval (true|false)", shortGuardInterval);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("useTcp", "Set true for TCP, false for UDP", useTcp);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
    cmd.AddValue("distance", "Distance between STA and AP (meters)", distance);
    cmd.AddValue("expectedThroughput", "Expected throughput (Mbps)", expectedThroughput);
    cmd.Parse(argc, argv);

    // Logging
    // LogComponentEnable ("BulkSendApplication", LOG_LEVEL_INFO);
    // LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);
    // LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Wi-Fi configuration
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());
    phy.Set("ShortGuardEnabled", BooleanValue(shortGuardInterval));
    phy.Set("ChannelWidth", UintegerValue(channelWidth));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

    std::ostringstream oss;
    oss << "HtMcs" << mcs;
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue(oss.str()),
                                "ControlMode", StringValue(oss.str()));

    WifiMacHelper mac;

    Ssid ssid = Ssid("ns3-80211n");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, wifiStaNode);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // RTS/CTS
    if (enableRtsCts)
    {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("0"));
    }
    else
    {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2347"));
    }

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(distance),
                                 "DeltaY", DoubleValue(0.0),
                                 "GridWidth", UintegerValue(2),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNode);
    Ptr<MobilityModel> staMob = wifiStaNode.Get(0)->GetObject<MobilityModel>();
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(0.0),
                                 "DeltaY", DoubleValue(0.0),
                                 "GridWidth", UintegerValue(1),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.Install(wifiApNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterface;
    staInterface = address.Assign(staDevice);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    // Application configuration
    uint16_t port = 9;

    ApplicationContainer sinkApp;
    if (useTcp)
    {
        Address sinkAddress(InetSocketAddress(apInterface.GetAddress(0), port));
        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        sinkApp = packetSinkHelper.Install(wifiApNode.Get(0));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(simulationTime + 1));

        BulkSendHelper bulkSender("ns3::TcpSocketFactory", sinkAddress);
        bulkSender.SetAttribute("MaxBytes", UintegerValue(0));
        bulkSender.SetAttribute("SendSize", UintegerValue(1472));
        ApplicationContainer sourceApp = bulkSender.Install(wifiStaNode.Get(0));
        sourceApp.Start(Seconds(1.0));
        sourceApp.Stop(Seconds(simulationTime + 1));
    }
    else
    {
        Address sinkAddress(InetSocketAddress(apInterface.GetAddress(0), port));
        PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        sinkApp = packetSinkHelper.Install(wifiApNode.Get(0));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(simulationTime + 1));

        OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
        onoff.SetConstantRate(DataRate(expectedThroughput * 1e6), 1472);
        onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
        onoff.SetAttribute("StopTime", TimeValue(Seconds(simulationTime + 1)));
        onoff.Install(wifiStaNode.Get(0));
    }

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 2.0));

    Simulator::Run();

    // Calculate and print goodput
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (auto it = stats.begin(); it != stats.end(); ++it)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
        if ((t.destinationAddress == apInterface.GetAddress(0) && t.sourceAddress == staInterface.GetAddress(0) && t.destinationPort == port)
            || (useTcp && t.sourceAddress == apInterface.GetAddress(0) && t.destinationAddress == staInterface.GetAddress(0)))
        {
            double duration = (it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds());
            if (duration > 0)
            {
                double goodput = it->second.rxBytes * 8.0 / duration / 1e6;
                std::cout << "Goodput: " << goodput << " Mbps" << std::endl;
            }
        }
    }

    Simulator::Destroy();
    return 0;
}