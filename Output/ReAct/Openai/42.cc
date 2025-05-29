#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-mac-helper.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HiddenWifiMpduAggregation");

int
main(int argc, char *argv[])
{
    bool enableRtsCts = false;
    uint32_t maxAmsdu = 1;
    uint32_t payloadSize = 1472;
    double simTime = 10.0;
    double throughputMin = 0.0;
    double throughputMax = 1000.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS mechanism", enableRtsCts);
    cmd.AddValue("maxAmsdu", "Number of aggregated MPDUs", maxAmsdu);
    cmd.AddValue("payloadSize", "Application payload size in bytes", payloadSize);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("throughputMin", "Expected throughput min bound (Mbps)", throughputMin);
    cmd.AddValue("throughputMax", "Expected throughput max bound (Mbps)", throughputMax);
    cmd.Parse(argc, argv);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);

    WifiMacHelper mac;
    Ssid ssid = Ssid("hidden-stations-ssid");

    // Enable MPDU aggregation
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("HtMcs7"),
                                "ControlMode", StringValue("HtMcs0"),
                                "MaxAmpduSize", UintegerValue(65535));

    // Enable/Disable RTS/CTS
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue(enableRtsCts ? "0" : "2200"));

    // MPDU aggregation count
    std::ostringstream ampduParam;
    ampduParam << maxAmsdu;
    Config::SetDefault("ns3::QosWifiMacHelper::MaxAmsduSize", UintegerValue(payloadSize * maxAmsdu));

    NetDeviceContainer staDevices;
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "QosSupported", BooleanValue(true));
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    NetDeviceContainer apDevice;
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "QosSupported", BooleanValue(true));
    apDevice = wifi.Install(phy, mac, wifiApNode);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

    // Position AP at (0, 0)
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    // Station 1 at (-50, 0)
    positionAlloc->Add(Vector(-50.0, 0.0, 0.0));
    // Station 2 at (50, 0)
    positionAlloc->Add(Vector(50.0, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNodes);

    // Limit Wi-Fi range to cause hidden node scenario
    phy.Set("RxGain", DoubleValue(-20));
    phy.Set("TxGain", DoubleValue(0));
    phy.Set("CcaMode1Threshold", DoubleValue(-62.0));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staIf = address.Assign(staDevices);
    Ipv4InterfaceContainer apIf = address.Assign(apDevice);

    // UDP server (PacketSink) on AP port 9
    uint16_t port = 9;
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                                InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sinkHelper.Install(wifiApNode.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simTime + 1));

    // UDP clients on stations
    UdpClientHelper client(apIf.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(10000000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.0003)));
    client.SetAttribute("PacketSize", UintegerValue(payloadSize));

    ApplicationContainer clientApps;
    clientApps.Add(client.Install(wifiStaNodes.Get(0)));
    clientApps.Add(client.Install(wifiStaNodes.Get(1)));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simTime + 1));

    // Enable pcap and ASCII traces
    phy.EnablePcapAll("hidden-wifi-80211n", false);
    std::ofstream asciiTraceFile;
    asciiTraceFile.open("hidden-wifi-80211n.tr");
    AsciiTraceHelper asciiTraceHelper;
    phy.EnableAsciiAll(asciiTraceHelper.CreateFileStream("hidden-wifi-80211n-phy.tr"));

    // FlowMonitor for throughput
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simTime + 1));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalThroughput_Mbps = 0.0;
    for (const auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        if (t.destinationAddress == apIf.GetAddress(0))
        {
            double throughput = flow.second.rxBytes * 8.0 / (simTime * 1000000.0); // Mbps
            totalThroughput_Mbps += throughput;
            std::cout << "Station: " << t.sourceAddress
                      << " -> AP: " << t.destinationAddress
                      << ", Rx bytes: " << flow.second.rxBytes
                      << ", Throughput: " << throughput << " Mbps" << std::endl;
        }
    }
    std::cout << "Total Throughput: " << totalThroughput_Mbps << " Mbps" << std::endl;
    std::cout << "Throughput bounds: [" << throughputMin << ", " << throughputMax << "] Mbps" << std::endl;
    if (totalThroughput_Mbps < throughputMin || totalThroughput_Mbps > throughputMax)
    {
        std::cout << "Warning: Throughput out of expected bounds." << std::endl;
    }

    Simulator::Destroy();
    asciiTraceFile.close();
    return 0;
}