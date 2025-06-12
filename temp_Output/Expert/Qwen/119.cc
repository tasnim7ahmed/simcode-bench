#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessAdHocNetwork");

int main(int argc, char *argv[]) {
    uint32_t maxPackets = 20;
    Time interPacketInterval = Seconds(0.5);
    double simulationTime = maxPackets * interPacketInterval.GetSeconds();

    NodeContainer nodes;
    nodes.Create(6);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    mac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(20.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        serverApps.Add(echoServer.Install(nodes.Get(i)));
    }
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime + 1.0));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        uint32_t nextNode = (i + 1) % nodes.GetN();
        UdpEchoClientHelper echoClient(interfaces.GetAddress(nextNode), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(maxPackets));
        echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));
        clientApps.Add(echoClient.Install(nodes.Get(i)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime + 1.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AnimationInterface anim("wireless_ad_hoc.xml");
    anim.EnablePacketMetadata(true);

    Simulator::Stop(Seconds(simulationTime + 2.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        std::cout << "Flow ID: " << iter->first << " Source: " << t.sourceAddress << " Destination: " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets: " << iter->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << iter->second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << iter->second.lostPackets << std::endl;
        std::cout << "  Packet Delivery Ratio: " << ((double)iter->second.rxPackets / (iter->second.txPackets ? iter->second.txPackets : 1)) << std::endl;
        std::cout << "  Average Delay: " << (iter->second.delaySum.GetSeconds() / (iter->second.rxPackets ? iter->second.rxPackets : 1)) << " s" << std::endl;
        std::cout << "  Throughput: " << (iter->second.rxBytes * 8.0 / simulationTime) / 1000.0 << " kbps" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}