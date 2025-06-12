#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-helper.h"
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridNetworkSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer wiredNodes;
    wiredNodes.Create(4);

    NodeContainer wirelessNodes;
    wirelessNodes.Create(3);

    // Connect wired nodes in a chain using point-to-point links
    std::vector<NetDeviceContainer> p2pDevices;
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    for (uint32_t i = 0; i < wiredNodes.GetN() - 1; ++i) {
        NetDeviceContainer devices = p2p.Install(wiredNodes.Get(i), wiredNodes.Get(i + 1));
        p2pDevices.push_back(devices);
    }

    // Setup Wi-Fi for wireless nodes
    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHz);

    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HtMcs7"),
                                 "ControlMode", StringValue("HtMcs0"));

    Ssid ssid = Ssid("ns-3-ssid");

    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(wifiPhy, wifiMac, wirelessNodes);

    // Set up AP node: one of the wired nodes acts as gateway (node 1)
    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice;
    apDevice = wifi.Install(wifiPhy, wifiMac, wiredNodes.Get(1));

    // Mobility model for wireless nodes
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(wirelessNodes);

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfaces;

    for (auto &devices : p2pDevices) {
        address.SetBase("10.1." + std::to_string(interfaces.size()) + ".0", "255.255.255.0");
        interfaces.push_back(address.Assign(devices));
    }

    address.SetBase("10.2.0.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup TCP traffic from wireless nodes to wired nodes
    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(interfaces[2].GetAddress(1), sinkPort));

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps;

    for (auto &node : wiredNodes) {
        sinkApps.Add(packetSinkHelper.Install(node));
    }
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    OnOffHelper onoff("ns3::TcpSocketFactory", Address());
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < wirelessNodes.GetN(); ++i) {
        onoff.SetAttribute("Remote", AddressValue(sinkAddress));
        clientApps.Add(onoff.Install(wirelessNodes.Get(i)));
    }

    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(9.0));

    // Enable pcap tracing
    wifiPhy.EnablePcap("wifi_ap", apDevice.Get(0), true, true);
    p2p.EnablePcapAll("wired_link", true);

    // Flow Monitor setup
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double totalThroughput = 0;
    uint32_t flowCount = 0;

    for (auto &[flowId, flowStats] : stats) {
        Ipv4FlowClassifier::FiveTuple t = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier())->FindFlow(flowId);
        if (t.protocol == 6) { // TCP protocol
            totalThroughput += (flowStats.rxBytes * 8.0) / (flowStats.timeLastRxPacket.GetSeconds() - flowStats.timeFirstTxPacket.GetSeconds()) / 1000 / 1000;
            flowCount++;
        }
    }

    if (flowCount > 0) {
        NS_LOG_UNCOND("Average TCP Throughput: " << totalThroughput / flowCount << " Mbps");
    } else {
        NS_LOG_UNCOND("No TCP flows recorded.");
    }

    Simulator::Destroy();
    return 0;
}