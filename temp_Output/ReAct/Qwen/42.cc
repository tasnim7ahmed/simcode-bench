#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HiddenStationsWiFi");

int main(int argc, char *argv[])
{
    bool enableRtsCts = false;
    uint32_t maxAmpduSize = 65535; // Default maximum AMPDU size
    double simulationTime = 10.0;
    uint32_t payloadSize = 1472;
    double throughputMin = 10.0;
    double throughputMax = 100.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
    cmd.AddValue("maxAmpduSize", "Maximum AMPDU size in bytes", maxAmpduSize);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("throughputMin", "Minimum expected throughput (Mbps)", throughputMin);
    cmd.AddValue("throughputMax", "Maximum expected throughput (Mbps)", throughputMax);
    cmd.Parse(argc, argv);

    // Set up WiFi MAC helper
    WifiMacHelper wifiMac;
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211n_5GHZ);
    wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));

    if (enableRtsCts)
    {
        wifiHelper.Set("RtsCtsThreshold", UintegerValue(0));
    }
    else
    {
        wifiHelper.Set("RtsCtsThreshold", UintegerValue(999999));
    }

    // Enable MPDU aggregation
    wifiHelper.SetBlockAckInactivityTimeout(0);
    wifiHelper.SetImmediateBlockAck(true);
    Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", UintegerValue(2200));
    Config::SetDefault("ns3::WifiRemoteStationManager::MaxAmpduSize", UintegerValue(maxAmpduSize));

    // Create nodes: AP + 2 stations
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Setup channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // Setup MAC
    Ssid ssid = Ssid("hidden-stations");
    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifiHelper.Install(phy, wifiMac, wifiStaNodes);

    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifiHelper.Install(phy, wifiMac, wifiApNode);

    // Mobility model - place stations so they are hidden from each other
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // Applications
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simulationTime));

    UdpClientHelper clientHelper(apInterface.GetAddress(0), port);
    clientHelper.SetAttribute("MaxPackets", UintegerValue(4294967295u)); // Infinite packets
    clientHelper.SetAttribute("Interval", TimeValue(Seconds(0.0001)));
    clientHelper.SetAttribute("PacketSize", UintegerValue(payloadSize));

    ApplicationContainer clientApp1 = clientHelper.Install(wifiStaNodes.Get(0));
    ApplicationContainer clientApp2 = clientHelper.Install(wifiStaNodes.Get(1));
    clientApp1.Start(Seconds(1.0));
    clientApp2.Start(Seconds(1.0));
    clientApp1.Stop(Seconds(simulationTime));
    clientApp2.Stop(Seconds(simulationTime));

    // PCAP tracing
    phy.EnablePcap("hidden_stations_ap", apDevice.Get(0));
    phy.EnablePcap("hidden_stations_sta1", staDevices.Get(0));
    phy.EnablePcap("hidden_stations_sta2", staDevices.Get(1));

    // ASCII tracing
    AsciiTraceHelper asciiTraceHelper;
    phy.EnableAsciiAll(asciiTraceHelper.CreateFileStream("hidden_stations.tr"));

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter)
    {
        Ipv4FlowClassifier::FiveTuple t = static_pointer_cast<Ipv4FlowClassifier>(flowmon.GetClassifier())->FindFlow(iter->first);
        std::cout << "Flow ID: " << iter->first << " Src Addr: " << t.sourceAddress << " Dst Addr: " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets: " << iter->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << iter->second.rxPackets << std::endl;
        std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / simulationTime / 1000000 << " Mbps" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}