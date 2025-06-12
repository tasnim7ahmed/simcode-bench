#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiSpatialReuse");

int main(int argc, char *argv[]) {
    bool enableObssPd = true;
    double d1 = 5.0; // STA1 - AP1 distance
    double d2 = 20.0; // AP1 - STA2 distance
    double d3 = 25.0; // AP2 - STA1 distance
    double txPower = 20.0; // dBm
    int channelWidth = 20; // MHz
    double sta1Ap1DataRate = 100; // Mbps
    double sta2Ap2DataRate = 100; // Mbps
    std::string sta1Ap1Interval = "1ms";
    std::string sta2Ap2Interval = "1ms";
    uint32_t packetSize = 1024;

    CommandLine cmd;
    cmd.AddValue("enableObssPd", "Enable OBSS PD spatial reuse (0 or 1)", enableObssPd);
    cmd.AddValue("d1", "STA1 - AP1 distance (meters)", d1);
    cmd.AddValue("d2", "AP1 - STA2 distance (meters)", d2);
    cmd.AddValue("d3", "AP2 - STA1 distance (meters)", d3);
    cmd.AddValue("txPower", "Transmit power (dBm)", txPower);
    cmd.AddValue("channelWidth", "Channel width (MHz)", channelWidth);
    cmd.AddValue("sta1Ap1DataRate", "STA1 to AP1 data rate (Mbps)", sta1Ap1DataRate);
    cmd.AddValue("sta2Ap2DataRate", "STA2 to AP2 data rate (Mbps)", sta2Ap2DataRate);
    cmd.AddValue("sta1Ap1Interval", "STA1 to AP1 interval", sta1Ap1Interval);
    cmd.AddValue("sta2Ap2Interval", "STA2 to AP2 interval", sta2Ap2Interval);
    cmd.AddValue("packetSize", "Packet size (bytes)", packetSize);
    cmd.Parse(argc, argv);

    LogComponentEnable("WifiSpatialReuse", LOG_LEVEL_INFO);

    // Configure wireless channel
    YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();

    // Configure channel width
    phyHelper.Set("ChannelWidth", UintegerValue(channelWidth));

    // Set up propagation loss
    YansWifiChannelConstLossModel lossModel;
    channelHelper.AddPropagationLoss("ns3::LogDistancePropagationLossModel");

    YansWifiChannel channel = channelHelper.Create();
    phyHelper.SetChannel(channel);

    // Configure Wi-Fi PHY
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211ax);

    // Configure spatial reuse
    if (enableObssPd) {
        Config::SetDefault("ns3::WifiMacQueue::MaxSize", QueueSizeValue(QueueSize(1000, "packets")));
        Config::SetDefault("ns3::WifiMacQueue::Mode", EnumValue(WifiMacQueue::ADAPTIVE));
        Config::SetDefault("ns3::WifiMacQueue::HighPriorityFlows", BooleanValue(true));

        // These values are just examples, adjust based on your scenario
        Config::SetDefault("ns3::WifiMac::EdcaQueues::Queue[TID_VO]::CwMin", UintegerValue(15));
        Config::SetDefault("ns3::WifiMac::EdcaQueues::Queue[TID_VO]::CwMax", UintegerValue(1023));

        Config::SetDefault("ns3::WifiMac::QosSupported", BooleanValue(true));
    }

    // Configure MAC layer
    WifiMacHelper macHelper;

    // Create two BSSes
    Ssid ssid1 = Ssid("BSS1");
    Ssid ssid2 = Ssid("BSS2");

    macHelper.SetType("ns3::ApWifiMac",
                      "Ssid", SsidValue(ssid1),
                      "BeaconInterval", TimeValue(Seconds(0.1)));
    NodeContainer apNodes1;
    apNodes1.Create(1);
    NetDeviceContainer apDevices1 = wifiHelper.Install(phyHelper, macHelper, apNodes1);

    macHelper.SetType("ns3::ApWifiMac",
                      "Ssid", SsidValue(ssid2),
                      "BeaconInterval", TimeValue(Seconds(0.1)));
    NodeContainer apNodes2;
    apNodes2.Create(1);
    NetDeviceContainer apDevices2 = wifiHelper.Install(phyHelper, macHelper, apNodes2);

    macHelper.SetType("ns3::StaWifiMac",
                      "Ssid", SsidValue(ssid1),
                      "ActiveProbing", BooleanValue(false));
    NodeContainer staNodes1;
    staNodes1.Create(1);
    NetDeviceContainer staDevices1 = wifiHelper.Install(phyHelper, macHelper, staNodes1);

    macHelper.SetType("ns3::StaWifiMac",
                      "Ssid", SsidValue(ssid2),
                      "ActiveProbing", BooleanValue(false));
    NodeContainer staNodes2;
    staNodes2.Create(1);
    NetDeviceContainer staDevices2 = wifiHelper.Install(phyHelper, macHelper, staNodes2);

    // Install mobility models
    MobilityHelper mobility;

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    NodeContainer allNodes;
    allNodes.Add(apNodes1);
    allNodes.Add(apNodes2);
    allNodes.Add(staNodes1);
    allNodes.Add(staNodes2);
    mobility.Install(allNodes);

    // Set node positions
    Ptr<Node> ap1 = apNodes1.Get(0);
    Ptr<Node> ap2 = apNodes2.Get(0);
    Ptr<Node> sta1 = staNodes1.Get(0);
    Ptr<Node> sta2 = staNodes2.Get(0);

    ap1->GetObject<MobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));
    sta1->GetObject<MobilityModel>()->SetPosition(Vector(d1, 0.0, 0.0));
    ap2->GetObject<MobilityModel>()->SetPosition(Vector(d2 + d3, 0.0, 0.0));
    sta2->GetObject<MobilityModel>()->SetPosition(Vector(d2 + d3 - d2, 0.0, 0.0));

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staApInterfaces1 = address.Assign(NetDeviceContainer::Create(staDevices1, apDevices1));
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer staApInterfaces2 = address.Assign(NetDeviceContainer::Create(staDevices2, apDevices2));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Configure applications
    uint16_t port1 = 9;
    uint16_t port2 = 10;

    OnOffHelper onoff1("ns3::UdpSocketFactory", Address(InetSocketAddress(staApInterfaces1.GetAddress(1), port1)));
    onoff1.SetConstantRate(DataRate(sta1Ap1DataRate + "Mbps"), packetSize);
    onoff1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer app1 = onoff1.Install(staNodes1.Get(0));
    app1.Start(Seconds(1.0));
    app1.Stop(Seconds(10.0));

    PacketSinkHelper sink1("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port1));
    ApplicationContainer sinkApp1 = sink1.Install(apNodes1.Get(0));
    sinkApp1.Start(Seconds(1.0));
    sinkApp1.Stop(Seconds(10.0));

    OnOffHelper onoff2("ns3::UdpSocketFactory", Address(InetSocketAddress(staApInterfaces2.GetAddress(1), port2)));
    onoff2.SetConstantRate(DataRate(sta2Ap2DataRate + "Mbps"), packetSize);
    onoff2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer app2 = onoff2.Install(staNodes2.Get(0));
    app2.Start(Seconds(1.0));
    app2.Stop(Seconds(10.0));

    PacketSinkHelper sink2("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port2));
    ApplicationContainer sinkApp2 = sink2.Install(apNodes2.Get(0));
    sinkApp2.Start(Seconds(1.0));
    sinkApp2.Stop(Seconds(10.0));

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Simulation time
    Simulator::Stop(Seconds(10.0));

    // Run simulation
    Simulator::Run();

    // Output results
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double throughput1 = 0.0;
    double throughput2 = 0.0;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.Seconds() - i->second.timeFirstTxPacket.Seconds()) / 1000000 << " Mbps\n";

        if (t.destinationAddress == staApInterfaces1.GetAddress(1)) {
            throughput1 = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.Seconds() - i->second.timeFirstTxPacket.Seconds()) / 1000000;
        } else if (t.destinationAddress == staApInterfaces2.GetAddress(1)) {
            throughput2 = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.Seconds() - i->second.timeFirstTxPacket.Seconds()) / 1000000;
        }
    }

    std::cout << "BSS1 Throughput: " << throughput1 << " Mbps\n";
    std::cout << "BSS2 Throughput: " << throughput2 << " Mbps\n";

    Simulator::Destroy();

    return 0;
}