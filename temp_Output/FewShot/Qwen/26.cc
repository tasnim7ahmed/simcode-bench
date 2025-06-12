#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

void RunSimulation(bool enableRtsCts);

int main(int argc, char *argv[]) {
    // Run simulation twice: once without RTS/CTS and once with
    RunSimulation(false);
    RunSimulation(true);

    return 0;
}

void RunSimulation(bool enableRtsCts) {
    // Set up logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes (Node 0, Node 1, Node 2)
    NodeContainer nodes;
    nodes.Create(3);

    // Setup Wi-Fi in ad-hoc mode
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    // Set up MAC layer
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    // Configure PHY
    YansWifiPhyHelper phy;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    
    // Use MatrixPropagationLossModel to simulate hidden terminal scenario
    Ptr<MatrixPropagationLossModel> lossModel = CreateObject<MatrixPropagationLossModel>();
    lossModel->SetDefaultLoss(200); // High loss by default (nodes not connected)

    // Set direct losses between adjacent nodes (node 0 <-> node 1, node 1 <-> node 2)
    lossModel->SetLoss(nodes.Get(0)->GetId(), nodes.Get(1)->GetId(), 50);
    lossModel->SetLoss(nodes.Get(1)->GetId(), nodes.Get(0)->GetId(), 50);
    lossModel->SetLoss(nodes.Get(1)->GetId(), nodes.Get(2)->GetId(), 50);
    lossModel->SetLoss(nodes.Get(2)->GetId(), nodes.Get(1)->GetId(), 50);

    phy.SetChannel(channel.Create());
    phy.Set("TxPowerStart", DoubleValue(16));
    phy.Set("TxPowerEnd", DoubleValue(16));
    phy.Set("TxGain", DoubleValue(0));
    phy.Set("RxGain", DoubleValue(0));
    phy.Set("LossModel", PointerValue(lossModel));

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Mobility for visualization purposes
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

    // Install Internet Stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Enable RTS/CTS if requested
    if (enableRtsCts) {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/$ns3::WifiRemoteStationManager/RtsCtsThreshold", UintegerValue(100));
    } else {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/$ns3::WifiRemoteStationManager/RtsCtsThreshold", UintegerValue(2200));
    }

    // Install OnOffApplications on Node 0 and Node 2 sending to Node 1
    uint16_t port = 9;

    OnOffHelper onoff1("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), port)));
    onoff1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff1.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff1.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer app1 = onoff1.Install(nodes.Get(0));
    app1.Start(Seconds(1.0));
    app1.Stop(Seconds(10.0));

    OnOffHelper onoff2("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), port)));
    onoff2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff2.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff2.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer app2 = onoff2.Install(nodes.Get(2));
    app2.Start(Seconds(1.1));
    app2.Stop(Seconds(10.0));

    // Install packet sink on Node 1
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Flow monitor setup
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Print statistics
    std::cout << "\n=== Simulation Run with RTS/CTS " << (enableRtsCts ? "Enabled" : "Disabled") << " ===\n";
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
    }

    Simulator::Destroy();
}