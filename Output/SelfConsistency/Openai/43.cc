#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wifi80211nMpduAggregationHiddenNodes");

void CheckThroughput(double expected, double delta, double measured)
{
    NS_LOG_UNCOND("Measured throughput: " << measured << " Mbps");
    if (std::abs(measured - expected) > delta)
    {
        NS_LOG_ERROR("Throughput check failed! Expected: " << expected
            << " ±" << delta << " Mbps, got: " << measured << " Mbps");
    }
    else
    {
        NS_LOG_UNCOND("Throughput within expected bounds.");
    }
}

int main(int argc, char *argv[])
{
    bool enableRtsCts = false;
    uint32_t maxAmpduSize = 65535; // bytes
    uint32_t payloadSize = 1472; // bytes
    double simulationTime = 10.0; // seconds
    double expectedThroughput = 50.0; // Mbps
    double throughputTolerance = 10.0; // Mbps

    CommandLine cmd(__FILE__);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS mechanism", enableRtsCts);
    cmd.AddValue("maxAmpduSize", "Max A-MPDU size (bytes)", maxAmpduSize);
    cmd.AddValue("payloadSize", "Payload size of UDP packets (bytes)", payloadSize);
    cmd.AddValue("simulationTime", "Simulation time (seconds)", simulationTime);
    cmd.AddValue("expectedThroughput", "Expected throughput (Mbps)", expectedThroughput);
    cmd.AddValue("throughputTolerance", "Allowed deviation (Mbps)", throughputTolerance);
    cmd.Parse(argc, argv);

    // Wifi
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // PHY & Channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(5.0));
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());
    phy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

    // MAC & Wifi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);

    // Remove rate adaptation for saturated UDP experiment (use constant rate)
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("HtMcs7"),
                                "ControlMode", StringValue("HtMcs0"));

    WifiMacHelper mac;
    Ssid ssid = Ssid("hidden-ssid");

    // Enable MPDU aggregation
    Config::SetDefault("ns3::WifiRemoteStationManager::MaxAmpduSize", UintegerValue(maxAmpduSize));
    Config::SetDefault("ns3::WifiRemoteStationManager::MaxAmsduSize", UintegerValue(7935)); // default

    // Enable/disable RTS/CTS
    if (enableRtsCts)
    {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(payloadSize));
    }
    else
    {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(999999));
    }

    // Station MACs
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // AP MAC
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility: AP at (0,0,0), hidden station 1 at (-3.5,0,0), hidden station 2 at (+3.5,0,0)
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
    posAlloc->Add(Vector(-3.5, 0.0, 0.0)); // STA 1
    posAlloc->Add(Vector(3.5, 0.0, 0.0)); // STA 2
    mobility.SetPositionAllocator(posAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);

    MobilityHelper mobilityAp;
    mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityAp.Install(wifiApNode);

    // Internet
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // Install UDP server on AP
    uint16_t port = 9009;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simulationTime + 1.0));

    // Install OnOffApplication (UDP traffic) on each STA (saturated)
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
    {
        UdpClientHelper client(apInterface.GetAddress(0), port);
        client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
        client.SetAttribute("Interval", TimeValue(Time("0.00001s"))); // as fast as possible
        client.SetAttribute("PacketSize", UintegerValue(payloadSize));
        ApplicationContainer app = client.Install(wifiStaNodes.Get(i));
        app.Start(Seconds(1.0));
        app.Stop(Seconds(simulationTime + 1.0));
        clientApps.Add(app);
    }

    // Enable pcap and ascii tracing
    phy.EnablePcapAll("hidden-wifi");
    AsciiTraceHelper ascii;
    phy.EnableAsciiAll(ascii.CreateFileStream("hidden-wifi.tr"));

    // Flow monitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 1.5));
    Simulator::Run();

    // Throughput calculation
    flowmon->CheckForLostPackets();
    double rxBytes = 0;
    double timeSpan = simulationTime; // traffic runs from 1.0 to simulationTime+1.0
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());

    std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();
    for (const auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        // Only count flows to the AP
        if (t.destinationAddress == apInterface.GetAddress(0) && t.destinationPort == port)
        {
            rxBytes += flow.second.rxBytes;
        }
    }
    // Throughput in Mbps
    double throughput = (rxBytes * 8.0) / (1e6 * timeSpan);
    CheckThroughput(expectedThroughput, throughputTolerance, throughput);

    Simulator::Destroy();
    return (std::abs(throughput - expectedThroughput) > throughputTolerance) ? 1 : 0;
}