#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdHocWirelessNetwork");

int main(int argc, char *argv[]) {
    uint32_t packetCount = 20;
    double intervalSeconds = 0.5;
    double simulationTime = packetCount * intervalSeconds + 1.0;
    std::string phyMode("DsssRate1Mbps");
    uint32_t numNodes = 4;
    double areaWidth = 200.0;
    double areaHeight = 200.0;

    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue(phyMode));

    NodeContainer nodes;
    nodes.Create(numNodes);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));

    mac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(areaWidth / 2),
                                  "DeltaY", DoubleValue(areaHeight / 2),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, areaWidth, 0, areaHeight)),
                              "Distance", DoubleValue(50.0));
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;
    for (uint32_t i = 0; i < numNodes; ++i) {
        uint32_t j = (i + 1) % numNodes;
        UdpEchoServerHelper server(port);
        ApplicationContainer serverApps = server.Install(nodes.Get(j));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(simulationTime));

        UdpEchoClientHelper client(interfaces.GetAddress(j), port);
        client.SetAttribute("MaxPackets", UintegerValue(packetCount));
        client.SetAttribute("Interval", TimeValue(Seconds(intervalSeconds)));
        client.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApps = client.Install(nodes.Get(i));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(simulationTime));
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AnimationInterface anim("adhoc_network.xml");
    anim.EnableIpv4RoutingTables(Seconds(0));
    anim.EnablePacketMetadata(true);

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = static_cast<Ipv4FlowClassifier*>(monitor->GetClassifier().Peek())->FindFlow(iter->first);
        std::cout << "Flow ID: " << iter->first << " Src Addr: " << t.sourceAddress << " Dst Addr: " << t.destinationAddress
                  << " Packets Delivery Ratio: " << (double)iter->second.rxPackets / (double)iter->second.txPackets
                  << " Avg Delay: " << iter->second.delaySum.GetSeconds() / iter->second.rxPackets
                  << " Throughput: " << (iter->second.rxBytes * 8.0) / simulationTime / 1000.0 << " kbps" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}