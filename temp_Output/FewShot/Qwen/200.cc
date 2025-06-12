#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set simulation parameters
    double simTime = 20.0;
    uint32_t numNodes = 4;
    std::string phyMode("DsssRate1Mbps");
    uint32_t packetSize = 1024;
    Time interPacketInterval = Seconds(1.0);
    std::string animFile = "aodv-wireless-animation.xml";
    std::string flowmonFile = "aodv-wireless-flowmon.xml";

    // Enable AODV logging
    LogComponentEnable("AodvRoutingProtocol", LOG_LEVEL_LOGIC);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Setup mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(nodes);

    // Configure WiFi
    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    wifiPhy.Set("ChannelNumber", UintegerValue(1));
    wifiPhy.Set("TxPowerEnd", DoubleValue(10.0));
    wifiPhy.Set("TxPowerStart", DoubleValue(10.0));
    wifiPhy.Set("RxGain", DoubleValue(0));
    wifiPhy.Set("CcaMode1Threshold", DoubleValue(-80));

    wifiMac.SetType("ns3::AdhocWifiMac");

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue(phyMode),
                                 "ControlMode", StringValue(phyMode));

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Install Internet stack with AODV
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Setup UDP traffic generator
    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(3), port)));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("100kbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer apps;
    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        Address sinkAddress(InetSocketAddress(interfaces.GetAddress(i), port));
        PacketSinkHelper packetSink("ns3::UdpSocketFactory", sinkAddress);
        apps.Add(packetSink.Install(nodes.Get(i)));
    }
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(simTime));

    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        onoff.SetAttribute("Remote", AddressValue(InetSocketAddress(interfaces.GetAddress(i), port)));
        ApplicationContainer clientApp = onoff.Install(nodes.Get(numNodes - 1));
        clientApp.Start(Seconds(2.0 + i));
        clientApp.Stop(Seconds(simTime));
    }

    // Setup FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Setup NetAnim
    AnimationInterface anim(animFile);
    anim.EnablePacketMetadata(true);

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Output flow statistics
    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = dynamic_cast<Ipv4FlowClassifier*>(flowmon.GetClassifier())->FindFlow(iter->first);
        std::cout << "Flow ID: " << iter->first << " Src Addr: " << t.sourceAddress << " Dst Addr: " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets: " << iter->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << iter->second.rxPackets << std::endl;
        std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / (simTime - 1) / 1000 << " kbps" << std::endl;
        if (iter->second.rxPackets > 0) {
            std::cout << "  Avg Delay: " << iter->second.delaySum.GetSeconds() / iter->second.rxPackets << " s" << std::endl;
        }
    }

    // Write flowmonitor file
    flowmon.SerializeToXmlFile(flowmonFile, false, false);

    Simulator::Destroy();
    return 0;
}