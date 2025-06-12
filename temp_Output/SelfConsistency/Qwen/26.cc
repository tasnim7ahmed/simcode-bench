#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HiddenTerminalExample");

int main(int argc, char *argv[]) {
    bool enableRtsCts = false; // Will be set per run

    CommandLine cmd(__FILE__);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS for packets larger than 100 bytes", enableRtsCts);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Create channels
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    Ptr<YansWifiChannel> wifiChannel = channel.Create();

    // Set propagation loss for hidden terminal scenario
    Ptr<MatrixPropagationLossModel> lossModel = CreateObject<MatrixPropagationLossModel>();
    lossModel->SetDefaultLoss(200); // High loss by default (no direct link between node 0 and 2)
    lossModel->SetLoss(nodes.Get(0)->GetId(), nodes.Get(1)->GetId(), 50); // 0 <-> 1
    lossModel->SetLoss(nodes.Get(1)->GetId(), nodes.Get(0)->GetId(), 50);
    lossModel->SetLoss(nodes.Get(1)->GetId(), nodes.Get(2)->GetId(), 50); // 1 <-> 2
    lossModel->SetLoss(nodes.Get(2)->GetId(), nodes.Get(1)->GetId(), 50);
    wifiChannel->SetPropagationLossModel(lossModel);

    // Setup Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    if (enableRtsCts) {
        wifi.MacAttributes("RtsThreshold", UintegerValue(100));
    } else {
        wifi.MacAttributes("RtsThreshold", UintegerValue(3000)); // Disable RTS/CTS
    }

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices;
    devices = wifi.Install(wifiChannel, mac, nodes);

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Applications
    uint16_t port = 9;

    OnOffHelper onoff1("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), port)));
    onoff1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff1.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff1.SetAttribute("PacketSize", UintegerValue(1000));

    ApplicationContainer app0 = onoff1.Install(nodes.Get(0));
    app0.Start(Seconds(1.0));
    app0.Stop(Seconds(10.0));

    OnOffHelper onoff2("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), port)));
    onoff2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff2.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff2.SetAttribute("PacketSize", UintegerValue(1000));

    ApplicationContainer app2 = onoff2.Install(nodes.Get(2));
    app2.Start(Seconds(1.1)); // Slightly staggered to avoid collisions at start
    app2.Stop(Seconds(10.0));

    // Sink application on node 1
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Print results
    std::cout << "\n=== Simulation Results (" << (enableRtsCts ? "RTS/CTS Enabled" : "RTS/CTS Disabled") << ") ===\n";
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << "):\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000 << " kbps\n";
    }

    Simulator::Destroy();
    return 0;
}