#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/on-off-application.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HiddenTerminal");

int main() {
    // Set up basic simulation parameters
    Time::SetResolution(Time::NS);
    LogComponent::SetAttribute("UdpClient", "Interval", StringValue("0.01"));

    // Create nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Configure Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Configure propagation loss model
    Ptr<MatrixPropagationLossModel> lossModel = CreateObject<MatrixPropagationLossModel>();
    lossModel->SetDefaultLoss(50); // dB loss between adjacent nodes
    wifiChannel.AddPropagationLossModel(lossModel);

    // Configure mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Configure CBR traffic: Node 0 -> Node 1
    uint16_t port0 = 9;
    OnOffHelper onoff0("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), port0)));
    onoff0.SetConstantRate(DataRate("1Mb/s"));
    onoff0.SetAttribute("PacketSize", UintegerValue(1000));

    ApplicationContainer app0 = onoff0.Install(nodes.Get(0));
    app0.Start(Seconds(1.0));
    app0.Stop(Seconds(10.0));

    // Configure CBR traffic: Node 2 -> Node 1
    uint16_t port2 = 9;
    OnOffHelper onoff2("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), port2)));
    onoff2.SetConstantRate(DataRate("1Mb/s"));
    onoff2.SetAttribute("PacketSize", UintegerValue(1000));

    ApplicationContainer app2 = onoff2.Install(nodes.Get(2));
    app2.Start(Seconds(1.1)); //Staggered start to avoid NS-3 bug.
    app2.Stop(Seconds(10.0));


    // Run simulation without RTS/CTS
    std::cout << "Simulation without RTS/CTS" << std::endl;

    FlowMonitorHelper flowmonHelper0;
    Ptr<FlowMonitor> monitor0 = flowmonHelper0.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor0->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier0 = DynamicCast<Ipv4FlowClassifier>(flowmonHelper0.GetClassifier());
    FlowMonitor::FlowStatsContainer stats0 = monitor0->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats0.begin(); i != stats0.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier0->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
    }

    Simulator::Destroy();

    // Run simulation with RTS/CTS enabled
    std::cout << "\nSimulation with RTS/CTS" << std::endl;
    Config::SetDefault("ns3::WifiMac::RtsCtsThreshold", StringValue("100"));

    FlowMonitorHelper flowmonHelper1;
    Ptr<FlowMonitor> monitor1 = flowmonHelper1.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor1->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier1 = DynamicCast<Ipv4FlowClassifier>(flowmonHelper1.GetClassifier());
    FlowMonitor::FlowStatsContainer stats1 = monitor1->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats1.begin(); i != stats1.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier1->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
    }


    Simulator::Destroy();

    return 0;
}