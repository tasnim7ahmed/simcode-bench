#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HiddenNodes80211n");

int main(int argc, char *argv[])
{
    bool enableRtsCts = false;
    uint32_t maxAmsduSize = 64000;
    uint32_t maxMpduSize = 64000;
    uint32_t payloadSize = 1000;
    double simulationTime = 10.0;
    double expectedThroughputLow = 0.0;
    double expectedThroughputHigh = 1e9;

    CommandLine cmd(__FILE__);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS for access point and stations", enableRtsCts);
    cmd.AddValue("maxAmsduSize", "Maximum AMSDU size in bytes", maxAmsduSize);
    cmd.AddValue("maxMpduSize", "Maximum MPDU size in bytes", maxMpduSize);
    cmd.AddValue("payloadSize", "UDP Payload size in bytes", payloadSize);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("expectedThroughputLow", "Expected minimum throughput (bps)", expectedThroughputLow);
    cmd.AddValue("expectedThroughputHigh", "Expected maximum throughput (bps)", expectedThroughputHigh);
    cmd.Parse(argc, argv);

    NodeContainer apNode;
    NodeContainer staNodes;
    apNode.Create(1);
    staNodes.Create(2);

    YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();
    phyHelper.SetChannel(channelHelper.Create());
    phyHelper.Set("TxPowerStart", DoubleValue(20));
    phyHelper.Set("TxPowerEnd", DoubleValue(20));
    phyHelper.Set("RxGain", DoubleValue(0));
    phyHelper.Set("ChannelNumber", UintegerValue(1));

    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211n_5MHZ);
    wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));

    WifiMacHelper macHelper;
    Ssid ssid = Ssid("hidden-node-network");
    macHelper.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "QosSupported", BooleanValue(true));
    NetDeviceContainer apDevice = wifiHelper.Install(phyHelper, macHelper, apNode);

    macHelper.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false), "QosSupported", BooleanValue(true));
    NetDeviceContainer staDevices = wifiHelper.Install(phyHelper, macHelper, staNodes);

    if (enableRtsCts)
    {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/RtsCtsThreshold", UintegerValue(1));
    }

    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/ShortGuardIntervalSupported", BooleanValue(true));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/LSigProtectionSupported", BooleanValue(true));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/MpduAggregator/MaxAmpduSize", UintegerValue(maxAmsduSize));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/MsduAggregator/MaxAmsduSize", UintegerValue(maxMpduSize));

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNodes);

    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(apInterface.GetAddress(0), port)));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < staNodes.GetN(); ++i)
    {
        onoff.SetAttribute("Remote", AddressValue(InetSocketAddress(apInterface.GetAddress(0), port)));
        clientApps.Add(onoff.Install(staNodes.Get(i)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime + 1.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = sink.Install(apNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simulationTime + 1.0));

    phyHelper.EnablePcapAll("hidden_nodes_wifi", false);
    AsciiTraceHelper asciiTraceHelper;
    phyHelper.EnableAsciiAll(asciiTraceHelper.CreateFileStream("hidden_nodes_wifi.tr"));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 2.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowStats> stats = monitor->GetFlowStats();

    double totalRxBytes = 0;
    for (std::pair<const FlowId, FlowStats> &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        if (t.destinationPort == port)
        {
            totalRxBytes += flow.second.rxBytes;
        }
    }

    double throughput = (totalRxBytes * 8.0) / simulationTime;
    NS_LOG_UNCOND("Measured Throughput: " << throughput << " bps");
    NS_ASSERT_MSG(throughput >= expectedThroughputLow && throughput <= expectedThroughputHigh,
                  "Throughput out of bounds!");

    Simulator::Destroy();
    return 0;
}