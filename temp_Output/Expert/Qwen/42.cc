#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HiddenStationsWiFi");

int main(int argc, char *argv[]) {
    bool enableRtsCts = false;
    uint32_t maxAmpduSize = 4095;
    double simulationTime = 10.0;
    uint32_t payloadSize = 1472;
    double expectedThroughputLow = 20.0;
    double expectedThroughputHigh = 60.0;

    CommandLine cmd;
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS handshake", enableRtsCts);
    cmd.AddValue("maxAmpduSize", "Maximum A-MPDU size (bytes)", maxAmpduSize);
    cmd.AddValue("simulationTime", "Total simulation time (seconds)", simulationTime);
    cmd.AddValue("payloadSize", "UDP Payload size in bytes", payloadSize);
    cmd.AddValue("throughputLow", "Expected lower bound of throughput (Mbps)", expectedThroughputLow);
    cmd.AddValue("throughputHigh", "Expected upper bound of throughput (Mbps)", expectedThroughputHigh);
    cmd.Parse(argc, argv);

    NodeContainer wifiApNode;
    wifiApNode.Create(1);
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    if (enableRtsCts) {
        wifi.Set("RtsCtsThreshold", UintegerValue(1));
    } else {
        wifi.Set("RtsCtsThreshold", UintegerValue(UINT32_MAX));
    }

    mac.SetType("ns3::ApWifiMac");
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(Ssid("hidden-network")),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNodes);

    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/$ns3::WifiRemoteStationManager/AmpduTxEnabled", BooleanValue(true));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/$ns3::WifiRemoteStationManager/MaxAmpduSize", UintegerValue(maxAmpduSize));

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);

    uint16_t port = 9;
    OnOffHelper onoff1("ns3::UdpSocketFactory", Address(InetSocketAddress(apInterface.GetAddress(0), port)));
    onoff1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff1.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    onoff1.SetAttribute("PacketSize", UintegerValue(payloadSize));

    ApplicationContainer app1;
    app1.Add(onoff1.Install(wifiStaNodes.Get(0)));
    app1.Start(Seconds(1.0));
    app1.Stop(Seconds(simulationTime));

    OnOffHelper onoff2("ns3::UdpSocketFactory", Address(InetSocketAddress(apInterface.GetAddress(0), port)));
    onoff2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff2.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    onoff2.SetAttribute("PacketSize", UintegerValue(payloadSize));

    ApplicationContainer app2;
    app2.Add(onoff2.Install(wifiStaNodes.Get(1)));
    app2.Start(Seconds(1.0));
    app2.Stop(Seconds(simulationTime)));

    AsciiTraceHelper ascii;
    phy.EnableAsciiAll(ascii.CreateFileStream("hidden-stations.tr"));
    phy.EnablePcapAll("hidden-stations");

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    double totalRxBytes = 0;
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        totalRxBytes += iter->second.rxBytes;
    }
    double throughput = (totalRxBytes * 8.0 / (simulationTime - 1.0)) / 1e6;
    NS_LOG_UNCOND("Throughput: " << throughput << " Mbps");
    NS_ASSERT_MSG(throughput >= expectedThroughputLow && throughput <= expectedThroughputHigh,
                  "Throughput out of expected bounds!");

    Simulator::Destroy();
    return 0;
}