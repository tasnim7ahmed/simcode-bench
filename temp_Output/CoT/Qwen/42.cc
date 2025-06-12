#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HiddenStationsWiFi");

int main(int argc, char *argv[]) {
    bool enableRtsCts = false;
    uint32_t maxAmpduSize = 65535; // Max AMPDU size in bytes
    double payloadSize = 1000; // Default payload size
    double simulationTime = 10.0; // Simulation time in seconds
    double throughputLowerBound = 0.0;
    double throughputUpperBound = 100.0; // Mbps

    CommandLine cmd(__FILE__);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS for Wi-Fi MAC", enableRtsCts);
    cmd.AddValue("maxAmpduSize", "Maximum size of aggregated MPDUs (bytes)", maxAmpduSize);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("simulationTime", "Simulation duration in seconds", simulationTime);
    cmd.AddValue("throughputLowerBound", "Expected minimum throughput (Mbps)", throughputLowerBound);
    cmd.AddValue("throughputUpperBound", "Expected maximum throughput (Mbps)", throughputUpperBound);
    cmd.Parse(argc, argv);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    Ptr<Node> apNode = CreateObject<Node>();

    YansWifiPhyHelper phy;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));

    WifiMacHelper mac;
    Ssid ssid = Ssid("hidden-station-network");
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "QosSupported", BooleanValue(true));
    if (enableRtsCts) {
        mac.Set("RTSThreshold", UintegerValue(0));
    } else {
        mac.Set("RTSThreshold", UintegerValue(2200)); // Disable RTS/CTS above this packet size
    }

    NetDeviceContainer apDevice;
    phy.SetPcapDataLinkType(SpectrumWifiPhyHelper::DLT_IEEE802_11_RADIO);
    apDevice = wifi.Install(phy, mac, apNode);

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false), "QosSupported", BooleanValue(true));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(wifiStaNodes);

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(apNode);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    UdpClientHelper client(staInterfaces.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295U));
    client.SetAttribute("Interval", TimeValue(Seconds(0.0001)));
    client.SetAttribute("PacketSize", UintegerValue(payloadSize));

    ApplicationContainer clientApps1 = client.Install(wifiStaNodes.Get(0));
    clientApps1.Start(Seconds(1.0));
    clientApps1.Stop(Seconds(simulationTime));

    ApplicationContainer clientApps2 = client.Install(wifiStaNodes.Get(1));
    clientApps2.Start(Seconds(1.0));
    clientApps2.Stop(Seconds(simulationTime));

    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/MaxAmpduSize", UintegerValue(maxAmpduSize));

    phy.EnablePcap("hidden_stations_ap", apDevice.Get(0));
    phy.EnablePcap("hidden_stations_sta1", staDevices.Get(0));
    phy.EnablePcap("hidden_stations_sta2", staDevices.Get(1));

    AsciiTraceHelper asciiTraceHelper;
    phy.EnableAsciiAll(asciiTraceHelper.CreateFileStream("hidden_stations.tr"));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    double totalThroughput = 0;
    for (auto & [flowId, flowStats] : stats) {
        Ipv4Address srcAddr, dstAddr;
        std::tie(srcAddr, dstAddr) = monitor->FindAddressesFromFlowId(flowId);
        if (dstAddr == staInterfaces.GetAddress(0) || dstAddr == staInterfaces.GetAddress(1)) continue;
        totalThroughput += (flowStats.rxBytes * 8.0 / (1e6 * simulationTime));
    }
    NS_LOG_UNCOND("Total Throughput: " << totalThroughput << " Mbps");
    NS_ASSERT_MSG(totalThroughput >= throughputLowerBound && totalThroughput <= throughputUpperBound,
                  "Throughput out of expected bounds!");

    Simulator::Destroy();
    return 0;
}