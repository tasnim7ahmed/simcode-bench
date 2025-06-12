#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridWiredWirelessExample");

static double
CalculateThroughput(Ptr<FlowMonitor> flowMonitor, FlowMonitorHelper& flowHelper)
{
    double averageThroughput = 0.0;
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
    uint32_t nFlows = 0;
    for (auto it = stats.begin(); it != stats.end(); ++it)
    {
        const FlowMonitor::FlowStats& flow = it->second;
        // Use only TCP flows
        Ipv4FlowClassifier::FiveTuple t = flowHelper.GetClassifier()->FindFlow(it->first);
        if (t.protocol == 6) // 6 == TCP
        {
            ++nFlows;
            double duration = (flow.timeLastRxPacket.GetSeconds() - flow.timeFirstTxPacket.GetSeconds());
            if (duration > 0)
            {
                double throughput = (flow.rxBytes * 8.0) / duration / 1e6; // Mbps
                averageThroughput += throughput;
            }
        }
    }
    if (nFlows > 0)
        averageThroughput /= nFlows;
    return averageThroughput;
}

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // 4 wired nodes, 3 wifi stations
    NodeContainer wiredNodes;
    wiredNodes.Create(4);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(3);

    NodeContainer wifiApNode = NodeContainer(wiredNodes.Get(1)); // Node 1 acts as Wi-Fi gateway

    // Point-to-point topology
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevs01 = p2p.Install(wiredNodes.Get(0), wiredNodes.Get(1));
    NetDeviceContainer p2pDevs12 = p2p.Install(wiredNodes.Get(1), wiredNodes.Get(2));
    NetDeviceContainer p2pDevs23 = p2p.Install(wiredNodes.Get(2), wiredNodes.Get(3));

    // Wi-Fi -- WiFi AP is wiredNodes.Get(1)
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevs = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDev = wifi.Install(phy, mac, wifiApNode);

    // Mobility for wifi nodes
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
        "MinX", DoubleValue(0.0),
        "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(5.0),
        "DeltaY", DoubleValue(5.0),
        "GridWidth", UintegerValue(3),
        "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(wiredNodes);
    stack.Install(wifiStaNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wiredIf01 = address.Assign(p2pDevs01);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer wiredIf12 = address.Assign(p2pDevs12);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer wiredIf23 = address.Assign(p2pDevs23);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiStaIf = address.Assign(staDevs);
    Ipv4InterfaceContainer wifiApIf = address.Assign(apDev);

    // Static routing
    Ipv4StaticRoutingHelper staticRouting;
    // For Wired Nodes
    for (uint32_t i = 0; i < wiredNodes.GetN(); ++i)
    {
        Ptr<Node> node = wiredNodes.Get(i);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        Ptr<Ipv4StaticRouting> routing = staticRouting.GetStaticRouting(ipv4);
        if (i == 0)
        {
            // Add route to WiFi subnet via Node 1
            routing->AddNetworkRouteTo(Ipv4Address("10.1.4.0"), Ipv4Mask("255.255.255.0"), wiredIf01.GetAddress(1), 1);
        }
        else if (i == 3)
        {
            routing->AddNetworkRouteTo(Ipv4Address("10.1.4.0"), Ipv4Mask("255.255.255.0"), wiredIf23.GetAddress(0), 1);
        }
        // Intermediate nodes can rely on direct connection for next hops
    }
    // For WiFi sta nodes
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
    {
        Ptr<Node> node = wifiStaNodes.Get(i);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        Ptr<Ipv4StaticRouting> routing = staticRouting.GetStaticRouting(ipv4);
        // Default route to AP
        routing->SetDefaultRoute(wifiApIf.GetAddress(0), 1);
    }
    // On AP node, add static routes to wired subnets
    Ptr<Node> apNode = wifiApNode.Get(0);
    Ptr<Ipv4> apIpv4 = apNode->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> apRouting = staticRouting.GetStaticRouting(apIpv4);
    // To wired0
    apRouting->AddNetworkRouteTo(Ipv4Address("10.1.1.0"), Ipv4Mask("255.255.255.0"), wiredIf01.GetAddress(0), 2);
    // To wired2
    apRouting->AddNetworkRouteTo(Ipv4Address("10.1.2.0"), Ipv4Mask("255.255.255.0"), wiredIf12.GetAddress(1), 3);
    // To wired3
    apRouting->AddNetworkRouteTo(Ipv4Address("10.1.3.0"), Ipv4Mask("255.255.255.0"), wiredIf12.GetAddress(1), 3);

    // Install TCP applications: BulkSend on WiFi sta, PacketSink on random wired node except gateway

    uint16_t port = 50000;
    double appStart = 1.0;
    double appStop = 10.0;
    ApplicationContainer sinkApps, sourceApps;

    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
    {
        Ptr<Node> sender = wifiStaNodes.Get(i);
        // Select one of the wired nodes as sink, avoid gateway (wiredNodes.Get(1))
        uint32_t destIndex = (i % 3 == 0) ? 0 : ((i % 3 == 1) ? 2 : 3);
        Ptr<Node> destNode = wiredNodes.Get(destIndex);
        Ipv4Address destAddr = destNode->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(); // interface 1 for up/down links
        Address sinkAddress(InetSocketAddress(destAddr, port + i));
        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port + i));
        sinkApps.Add(packetSinkHelper.Install(destNode));
        BulkSendHelper source("ns3::TcpSocketFactory", sinkAddress);
        source.SetAttribute("MaxBytes", UintegerValue(0));
        source.SetAttribute("SendSize", UintegerValue(1448));
        sourceApps.Add(source.Install(sender));
    }

    sinkApps.Start(Seconds(appStart));
    sinkApps.Stop(Seconds(appStop));
    sourceApps.Start(Seconds(appStart));
    sourceApps.Stop(Seconds(appStop));

    // Enable PCAP tracing
    p2p.EnablePcapAll("hybrid-wired");
    phy.EnablePcap("hybrid-wifi-ap", apDev.Get(0));
    for (uint32_t i = 0; i < staDevs.GetN(); ++i)
    {
        phy.EnablePcap("hybrid-wifi-sta", staDevs.Get(i));
    }

    // Flow monitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(appStop + 1));
    Simulator::Run();

    double avgThroughput = CalculateThroughput(flowmon, flowmonHelper);

    std::cout << "Average TCP Flow Throughput: " << avgThroughput << " Mbps" << std::endl;

    Simulator::Destroy();
    return 0;
}