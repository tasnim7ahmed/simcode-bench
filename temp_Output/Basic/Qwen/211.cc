#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessAdHocSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Enable logging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(5);

    // Setup WiFi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(nodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up UDP traffic from node 0 to node 1
    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), port)));
    onoff.SetConstantRate(DataRate("1Mbps"), 1024);
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(10.0)));

    ApplicationContainer apps = onoff.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Sink application on node 1
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    apps = sink.Install(nodes.Get(1));
    apps.Start(Seconds(0.0));

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Flow monitor setup
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("ad-hoc-wireless.tr");
    phy.EnableAsciiAll(stream);

    // Simulation stop time
    Simulator::Stop(Seconds(11.0));

    // Run simulation
    Simulator::Run();

    // Flow monitor output
    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = static_cast<Ipv4FlowClassifier*>(monitor->GetClassifier().Peek())->FindFlow(iter->first);
        if (t.sourceAddress == interfaces.GetAddress(0) && t.destinationAddress == interfaces.GetAddress(1)) {
            std::ofstream outFile("results.txt", std::ios::app);
            outFile << "Flow: " << t.sourceAddress << " -> " << t.destinationAddress << "\n";
            outFile << "Packet Loss: " << iter->second.lostPackets << "\n";
            outFile << "Throughput: " << (iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds())) / 1000000 << " Mbps\n";
            outFile.close();
        }
    }

    Simulator::Destroy();
    return 0;
}