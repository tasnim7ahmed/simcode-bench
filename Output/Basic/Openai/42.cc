#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-mac-helper.h"
#include "ns3/wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/packet-sink.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiHiddenStationMpduAggregation");

void ThroughputMonitor(Ptr<FlowMonitor> monitor, double simulationTime)
{
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    double throughput = 0.0;
    for (auto iter = stats.begin(); iter != stats.end(); ++iter)
    {
        if (iter->second.rxPackets > 0)
        {
            throughput += (iter->second.rxBytes * 8.0) / (simulationTime * 1000000.0); // Mbps
        }
    }
    std::cout << "Average throughput: " << throughput << " Mbps" << std::endl;
}

int main(int argc, char *argv[])
{
    double simulationTime = 10.0;
    uint32_t payloadSize = 1472; // bytes
    bool enableRtsCts = false;
    uint32_t maxAmpduSize = 64; // Number of MPDUs in an A-MPDU
    double minExpectedThroughput = 0.0;
    double maxExpectedThroughput = 1000.0;

    CommandLine cmd;
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS (true/false)", enableRtsCts);
    cmd.AddValue("maxAmpduSize", "Maximum number of MPDUs in an A-MPDU", maxAmpduSize);
    cmd.AddValue("minExpectedThroughput", "Lower bound throughput to assert in Mbps", minExpectedThroughput);
    cmd.AddValue("maxExpectedThroughput", "Upper bound throughput to assert in Mbps", maxExpectedThroughput);
    cmd.Parse(argc, argv);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

    WifiMacHelper mac;
    Ssid ssid = Ssid("hidden-ssid");

    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("HtMcs7"),
                                "ControlMode", StringValue("HtMcs0"));

    // MPDU Aggregation configuration
    Config::SetDefault("ns3::EdcaTxopN::MaxAmpduSize", UintegerValue(maxAmpduSize * payloadSize));
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(enableRtsCts ? 0 : 2347));

    // Station MACs
    NetDeviceContainer staDevices;
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // AP MAC
    NetDeviceContainer apDevice;
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Positioning: Stations (n1 and n2) are out of range of each other, both in range of the AP.
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
    posAlloc->Add(Vector(0.0, 0.0, 0.0));    // AP at (0,0)
    posAlloc->Add(Vector(50.0, 0.0, 0.0));   // STA1 at (50,0)
    posAlloc->Add(Vector(-50.0, 0.0, 0.0));  // STA2 at (-50,0)
    mobility.SetPositionAllocator(posAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    NodeContainer allNodes;
    allNodes.Add(wifiApNode);
    allNodes.Add(wifiStaNodes);
    mobility.Install(allNodes);

    // Set a low propagation range to create hidden nodes
    phy.Set("RxGain", DoubleValue(0));
    phy.Set("TxGain", DoubleValue(0));
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(60.0));

    InternetStackHelper stack;
    stack.Install(allNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // Install applications: each STA sends UDP to AP
    uint16_t udpPort = 5000;
    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    UdpServerHelper udpServer(udpPort);
    serverApps.Add(udpServer.Install(wifiApNode.Get(0)));

    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
    {
        UdpClientHelper udpClient(apInterface.GetAddress(0), udpPort);
        udpClient.SetAttribute("MaxPackets", UintegerValue(0));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.0005)));
        udpClient.SetAttribute("PacketSize", UintegerValue(payloadSize));
        clientApps.Add(udpClient.Install(wifiStaNodes.Get(i)));
    }

    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime));

    // Enable tracing
    phy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11);
    phy.EnablePcapAll("wifi-hidden-aggregation", false);
    AsciiTraceHelper ascii;
    phy.EnableAsciiAll(ascii.CreateFileStream("wifi-hidden-aggregation.tr"));

    // Flow Monitor
    Ptr<FlowMonitor> monitor;
    FlowMonitorHelper flowmon;
    monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Throughput reporting
    monitor->CheckForLostPackets();
    ThroughputMonitor(monitor, simulationTime - 1.0);

    // Assert throughput if bounds provided
    double throughput = 0.0;
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter)
    {
        throughput += (iter->second.rxBytes * 8.0) / ((simulationTime-1.0) * 1000000.0);
    }
    if (minExpectedThroughput > 0.0 && throughput < minExpectedThroughput)
    {
        NS_FATAL_ERROR("Throughput below minimum expected: " << throughput << " Mbps");
    }
    if (maxExpectedThroughput > 0.0 && throughput > maxExpectedThroughput)
    {
        NS_FATAL_ERROR("Throughput above maximum expected: " << throughput << " Mbps");
    }

    Simulator::Destroy();
    return 0;
}