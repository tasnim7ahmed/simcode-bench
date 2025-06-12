#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessAdHocNetworkSimulation");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 4;
    double simulationTime = 10.0;
    std::string phyMode("DsssRate1Mbps");

    NodeContainer nodes;
    nodes.Create(numNodes);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv);
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(nodes);

    AnimationInterface anim("wireless-adhoc-animation.xml");
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        anim.UpdateNodeDescription(nodes.Get(i), "Node-" + std::to_string(i));
        anim.UpdateNodeColor(nodes.Get(i), 255, 0, 0);
    }

    ApplicationContainer apps;
    uint16_t port = 9;

    for (uint32_t i = 0; i < numNodes; ++i) {
        for (uint32_t j = 0; j < numNodes; ++j) {
            if (i != j) {
                UdpEchoServerHelper echoServer(port);
                apps = echoServer.Install(nodes.Get(j));
                apps.Start(Seconds(1.0));
                apps.Stop(Seconds(simulationTime));

                UdpEchoClientHelper echoClient(interfaces.GetAddress(j), port);
                echoClient.SetAttribute("MaxPackets", UintegerValue(20));
                echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
                echoClient.SetAttribute("PacketSize", UintegerValue(512));

                apps = echoClient.Install(nodes.Get(i));
                apps.Start(Seconds(2.0));
                apps.Stop(Seconds(simulationTime));
            }
        }
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double totalPDR = 0.0;
    double totalThroughput = 0.0;
    double totalDelay = 0.0;
    uint32_t flowCount = 0;

    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = static_cast<Ipv4FlowClassifier*>(flowmon.GetClassifier().Peek())->FindFlow(iter->first);
        std::cout << "Flow ID: " << iter->first << " Src Addr: " << t.sourceAddress << " Dst Addr: " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets = " << iter->second.txPackets << std::endl;
        std::cout << "  Rx Packets = " << iter->second.rxPackets << std::endl;
        std::cout << "  Lost Packets = " << iter->second.lostPackets << std::endl;
        std::cout << "  Throughput: " << (iter->second.rxBytes * 8.0) / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps" << std::endl;
        std::cout << "  Mean Delay: " << iter->second.delaySum.GetSeconds() / iter->second.rxPackets << " s" << std::endl;

        if (iter->second.rxPackets > 0) {
            double pdr = (double)iter->second.rxPackets / (double)(iter->second.txPackets);
            double throughput = (iter->second.rxBytes * 8.0) / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1000000;
            double avgDelay = iter->second.delaySum.GetSeconds() / iter->second.rxPackets;

            totalPDR += pdr;
            totalThroughput += throughput;
            totalDelay += avgDelay;
            flowCount++;
        }
    }

    if (flowCount > 0) {
        std::cout << "\n--- Overall Metrics ---" << std::endl;
        std::cout << "Average Packet Delivery Ratio: " << (totalPDR / flowCount) << std::endl;
        std::cout << "Average Throughput: " << (totalThroughput / flowCount) << " Mbps" << std::endl;
        std::cout << "Average End-to-End Delay: " << (totalDelay / flowCount) << " s" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}