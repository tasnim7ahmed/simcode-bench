#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);

    // Wired nodes: n0 <-> n1 <-> n2 <-> n3
    NodeContainer wiredNodes;
    wiredNodes.Create(4);

    // Point-to-point links between wired nodes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevs01 = p2p.Install(wiredNodes.Get(0), wiredNodes.Get(1));
    NetDeviceContainer p2pDevs12 = p2p.Install(wiredNodes.Get(1), wiredNodes.Get(2));
    NetDeviceContainer p2pDevs23 = p2p.Install(wiredNodes.Get(2), wiredNodes.Get(3));

    // Wireless nodes (station nodes + gateway)
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(3); // wireless station nodes

    NodeContainer wifiApNode = NodeContainer(wiredNodes.Get(1)); // gateway: n1

    // Wi-Fi configuration
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-ssid");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevs = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevs = wifi.Install(phy, mac, wifiApNode);

    // Mobility for stations and AP
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(10.0),
                                 "DeltaY", DoubleValue(10.0),
                                 "GridWidth", UintegerValue(3),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wiredNodes);
    stack.Install(wifiStaNodes);

    // IP address assignment
    Ipv4AddressHelper ipv4;

    // Point-to-point subnets
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i01 = ipv4.Assign(p2pDevs01);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i12 = ipv4.Assign(p2pDevs12);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i23 = ipv4.Assign(p2pDevs23);

    // WiFi subnet
    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer staIfs = ipv4.Assign(staDevs);
    Ipv4InterfaceContainer apIfs = ipv4.Assign(apDevs);

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // PCAP tracing
    p2p.EnablePcapAll("hybrid-wired");
    phy.EnablePcap("hybrid-wifi", apDevs.Get(0));
    phy.EnablePcap("hybrid-wifi", staDevs);

    uint16_t tcpPort = 50000;
    double appStart = 1.0;
    double appStop = 10.0;

    // Install BulkSendApplication (TCP source: each wifi station sends to a distinct wired node)
    ApplicationContainer sources;
    ApplicationContainer sinks;
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
    {
        Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), tcpPort + i));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);

        // Sink installed on a wired node (for diversity: cycle through n0, n2, n3)
        Ptr<Node> wiredSinkNode = wiredNodes.Get((i == 0) ? 0 : (i == 1) ? 2 : 3);
        ApplicationContainer sinkApp = sinkHelper.Install(wiredSinkNode);
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(appStop + 1.0));
        sinks.Add(sinkApp);

        // The address to send to is the wired node's interface toward its internal subnet
        Ipv4Address serverAddr;
        if (i == 0)
            serverAddr = i01.GetAddress(0); // n0
        else if (i == 1)
            serverAddr = i12.GetAddress(1); // n2
        else
            serverAddr = i23.GetAddress(1); // n3

        BulkSendHelper source("ns3::TcpSocketFactory",
                              InetSocketAddress(serverAddr, tcpPort + i));
        source.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited
        source.SetAttribute("SendSize", UintegerValue(1024));

        ApplicationContainer sourceApp = source.Install(wifiStaNodes.Get(i));
        sourceApp.Start(Seconds(appStart));
        sourceApp.Stop(Seconds(appStop));
        sources.Add(sourceApp);
    }

    // FlowMonitor setup
    Ptr<FlowMonitor> flowmon;
    FlowMonitorHelper flowmonHelper;
    flowmon = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(appStop + 1.0));
    Simulator::Run();

    flowmon->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();

    double totalThroughput = 0.0;
    uint32_t flowCount = 0;

    for (auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        if (t.destinationPort >= tcpPort && t.destinationPort < tcpPort + 3)
        {
            double throughput = (flow.second.rxBytes * 8.0) / (appStop - appStart) / 1e6; // Mbps
            totalThroughput += throughput;
            flowCount++;
        }
    }

    if (flowCount > 0)
    {
        std::cout << "Average TCP Flow Throughput: "
                  << (totalThroughput / flowCount)
                  << " Mbps" << std::endl;
    }
    else
    {
        std::cout << "No TCP flows detected." << std::endl;
    }

    Simulator::Destroy();
    return 0;
}