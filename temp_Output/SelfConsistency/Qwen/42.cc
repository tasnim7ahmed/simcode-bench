#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HiddenStationsWiFi");

int main(int argc, char *argv[])
{
    // Default parameters
    bool enableRtsCts = false;
    uint32_t maxAmpduSize = 65535; // Maximum AMPDU size in bytes
    double simulationTime = 10.0;
    uint32_t payloadSize = 1472;
    double throughputLowBound = 20.0;
    double throughputHighBound = 100.0;
    bool pcapTracing = true;
    bool asciiTracing = true;

    CommandLine cmd;
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS for Wi-Fi MAC", enableRtsCts);
    cmd.AddValue("maxAmpduSize", "Maximum AMPDU size (bytes)", maxAmpduSize);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("payloadSize", "Payload size of UDP packets", payloadSize);
    cmd.AddValue("throughputLowBound", "Lower bound on expected throughput (Mbps)", throughputLowBound);
    cmd.AddValue("throughputHighBound", "Upper bound on expected throughput (Mbps)", throughputHighBound);
    cmd.AddValue("pcapTracing", "Enable PCAP tracing", pcapTracing);
    cmd.AddValue("asciiTracing", "Enable ASCII tracing", asciiTracing);
    cmd.Parse(argc, argv);

    NS_LOG_INFO("Creating nodes.");
    NodeContainer wifiApNode;
    wifiApNode.Create(1);
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                  "DataMode", StringValue("HtMcs7"),
                                  "ControlMode", StringValue("HtMcs0"));

    WifiMacHelper mac;
    Ssid ssid = Ssid("hidden-stations-network");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = mac.Install(phy, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconGeneration", BooleanValue(true),
                "BeaconInterval", TimeValue(Seconds(2.5)));

    NetDeviceContainer apDevice;
    apDevice = mac.Install(phy, wifiApNode);

    // Enable AMPDU aggregation
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/EnableMpduAggregation", BooleanValue(true));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/MaxAmpduSize", UintegerValue(maxAmpduSize));

    if (enableRtsCts)
    {
        wifiStaNodes.Get(0)->GetDevice(0)->GetObject<WifiNetDevice>()->GetMac()->SetAttribute("RtsCtsEnabled", BooleanValue(true));
        wifiStaNodes.Get(1)->GetDevice(0)->GetObject<WifiNetDevice>()->GetMac()->SetAttribute("RtsCtsEnabled", BooleanValue(true));
    }

    NS_LOG_INFO("Setting mobility model to static.");
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNodes);

    // Adjust positions to simulate hidden node scenario
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // AP at origin
    positionAlloc->Add(Vector(-10.0, 10.0, 0.0)); // STA1
    positionAlloc->Add(Vector(10.0, 10.0, 0.0));  // STA2 (hidden from STA1)
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(wifiApNode.Get(0));
    mobility.Install(wifiStaNodes);

    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);

    NS_LOG_INFO("Installing applications.");

    uint16_t port = 9;
    OnOffHelper onoff1("ns3::UdpSocketFactory", Address(InetSocketAddress(apInterface.GetAddress(0), port)));
    onoff1.SetConstantRate(DataRate("100Mbps"));
    onoff1.SetAttribute("PacketSize", UintegerValue(payloadSize));

    ApplicationContainer app1 = onoff1.Install(wifiStaNodes.Get(0));
    app1.Start(Seconds(1.0));
    app1.Stop(Seconds(simulationTime - 1.0));

    OnOffHelper onoff2("ns3::UdpSocketFactory", Address(InetSocketAddress(apInterface.GetAddress(0), port)));
    onoff2.SetConstantRate(DataRate("100Mbps"));
    onoff2.SetAttribute("PacketSize", UintegerValue(payloadSize));

    ApplicationContainer app2 = onoff2.Install(wifiStaNodes.Get(1));
    app2.Start(Seconds(1.0));
    app2.Stop(Seconds(simulationTime - 1.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(wifiApNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simulationTime));

    if (pcapTracing)
    {
        phy.EnablePcapAll("hidden_stations_wifi");
    }

    if (asciiTracing)
    {
        AsciiTraceHelper ascii;
        phy.EnableAsciiAll(ascii.CreateFileStream("hidden_stations_wifi.tr"));
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    NS_LOG_INFO("Running simulation...");
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowStats> stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if (t.destinationPort == 9)
        {
            double throughput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
            NS_LOG_UNCOND("Flow ID: " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")");
            NS_LOG_UNCOND("Throughput: " << throughput << " Mbps");
            NS_ASSERT_MSG(throughput >= throughputLowBound && throughput <= throughputHighBound,
                          "Throughput out of expected bounds!");
        }
    }

    Simulator::Destroy();
    return 0;
}