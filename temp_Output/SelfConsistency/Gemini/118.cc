#include <iostream>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocGridSimulation");

int main(int argc, char *argv[]) {
    bool enable_flow_monitor = true;
    bool enable_netanim = true;

    CommandLine cmd;
    cmd.AddValue("flowMonitor", "Enable Flow Monitor", enable_flow_monitor);
    cmd.AddValue("netanim", "Enable Netanim", enable_netanim);
    cmd.Parse(argc, argv);

    LogComponent::SetLogLevel(LOG_LEVEL_INFO);

    // Create Nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Configure Wifi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());

    NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Configure Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomBoxPositionAllocator",
                              "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                              "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                              "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));

    mobility.Install(nodes);

    // Install Internet Stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IPv4 Addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Configure UDP Echo Applications
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps[4];
    for (int i = 0; i < 4; ++i) {
        serverApps[i] = echoServer.Install(nodes.Get(i));
        serverApps[i].Start(Seconds(1.0));
        serverApps[i].Stop(Seconds(10.0));
    }

    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(20));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps[4];
    clientApps[0] = echoClient.Install(nodes.Get(0));
    clientApps[0].Start(Seconds(2.0));
    clientApps[0].Stop(Seconds(10.0));

    echoClient.SetAttribute("RemoteAddress", AddressValue(interfaces.GetAddress(2)));
    clientApps[1] = echoClient.Install(nodes.Get(1));
    clientApps[1].Start(Seconds(2.0));
    clientApps[1].Stop(Seconds(10.0));

    echoClient.SetAttribute("RemoteAddress", AddressValue(interfaces.GetAddress(3)));
    clientApps[2] = echoClient.Install(nodes.Get(2));
    clientApps[2].Start(Seconds(2.0));
    clientApps[2].Stop(Seconds(10.0));

    echoClient.SetAttribute("RemoteAddress", AddressValue(interfaces.GetAddress(0)));
    clientApps[3] = echoClient.Install(nodes.Get(3));
    clientApps[3].Start(Seconds(2.0));
    clientApps[3].Stop(Seconds(10.0));

    // Enable Flow Monitor
    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor;
    if (enable_flow_monitor) {
        monitor = flowMonitor.InstallAll();
    }

    // Enable NetAnim
    if (enable_netanim) {
        AnimationInterface anim("adhoc-grid.xml");
        anim.SetMaxPktsPerTraceFile(500000);
    }

    // Run Simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Analyze and Output Results
    if (enable_flow_monitor) {
        monitor->CheckForLostPackets();
        Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
        FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

        for (auto i = stats.begin(); i != stats.end(); ++i) {
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
            std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
            std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
            std::cout << "  Packet Loss Ratio: " << (double)(i->second.txPackets - i->second.rxPackets) / (double)i->second.txPackets << "\n";
            std::cout << "  End-to-End Delay: " << i->second.delaySum << "\n";
            std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
        }
    }

    Simulator::Destroy();
    return 0;
}