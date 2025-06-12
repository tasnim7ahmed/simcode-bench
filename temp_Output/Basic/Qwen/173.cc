#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridNetworkSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    // Create wired nodes
    NodeContainer wiredNodes;
    wiredNodes.Create(4);

    // Create point-to-point links between wired nodes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer wiredDevices;
    for (uint32_t i = 0; i < wiredNodes.GetN() - 1; ++i) {
        NetDeviceContainer devices = p2p.Install(wiredNodes.Get(i), wiredNodes.Get(i + 1));
        wiredDevices.Add(devices);
    }

    // Install Internet stack on wired nodes
    InternetStackHelper internet;
    internet.Install(wiredNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wiredInterfaces = ipv4.Assign(wiredDevices);

    // Create wireless nodes
    NodeContainer wirelessNodes;
    wirelessNodes.Create(3);

    // Create Wi-Fi channel and PHY
    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    // Set Wi-Fi MAC and PHY
    WifiMacHelper wifiMac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::AarfcdWifiManager");

    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(Ssid("wifi-network")));
    NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, wirelessNodes);

    // Setup AP node (use one of the wired nodes as gateway)
    NodeContainer apNode = wiredNodes.Get(0); // First wired node is AP
    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(Ssid("wifi-network")));
    NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, apNode);

    // Mobility model for wireless nodes
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wirelessNodes);
    mobility.Install(apNode);

    // Install Internet stack on wireless nodes and AP
    internet.Install(wirelessNodes);
    internet.Install(apNode);

    // Assign IP addresses to wireless and AP
    Ipv4InterfaceContainer wirelessInterfaces;
    Ipv4InterfaceContainer apInterface;

    ipv4.SetBase("10.2.1.0", "255.255.255.0");
    wirelessInterfaces = ipv4.Assign(staDevices);
    apInterface = ipv4.Assign(apDevice);

    // Enable pcap tracing
    wifiPhy.EnablePcap("ap_device", apDevice.Get(0));
    for (uint32_t i = 0; i < staDevices.GetN(); ++i) {
        std::ostringstream oss;
        oss << "wireless_node_" << i;
        wifiPhy.EnablePcap(oss.str(), staDevices.Get(i));
    }

    // Setup routing so wireless network can reach wired
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup TCP traffic from wireless nodes to wired nodes
    uint16_t port = 5000;
    ApplicationContainer sinkApps;
    ApplicationContainer sourceApps;

    for (uint32_t i = 0; i < wirelessNodes.GetN(); ++i) {
        Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
        sinkApps.Add(packetSinkHelper.Install(wiredNodes.Get(3))); // last wired node as receiver

        InetSocketAddress remoteAddress(wiredInterfaces.GetAddress(3), port);
        remoteAddress.SetTos(0xb8); // DSCP AF41 = 0xb8

        BulkSendHelper bulkSend("ns3::TcpSocketFactory", remoteAddress);
        bulkSend.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited
        sourceApps.Add(bulkSend.Install(wirelessNodes.Get(i)));
    }

    sinkApps.Start(Seconds(1.0));
    sourceApps.Start(Seconds(2.0));
    sourceApps.Stop(Seconds(10.0));

    // Flow monitor setup
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double totalThroughput = 0;
    uint32_t flowCount = 0;

    for (auto & [flowId, flowStats] : stats) {
        if (flowStats.rxBytes > 0) {
            double duration = (flowStats.timeLastRxPacket.GetSeconds() - flowStats.timeFirstTxPacket.GetSeconds());
            double throughput = (flowStats.rxBytes * 8.0) / duration / 1e6; // Mbps
            totalThroughput += throughput;
            flowCount++;
        }
    }

    if (flowCount > 0) {
        NS_LOG_UNCOND("Average Throughput: " << (totalThroughput / flowCount) << " Mbps");
    } else {
        NS_LOG_UNCOND("No traffic received.");
    }

    Simulator::Destroy();
    return 0;
}