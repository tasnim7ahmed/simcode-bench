#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdHocWirelessNetwork");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 4;
    double duration = 10.0;
    uint32_t packetSize = 512;
    uint32_t maxPackets = 20;
    Time interPacketInterval = Seconds(0.5);

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    YansWifiPhyHelper wifiPhy;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    wifiPhy.Set("ChannelWidth", UintegerValue(20));

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(20.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    uint16_t port = 9;

    for (uint32_t i = 0; i < numNodes; ++i) {
        uint32_t j = (i + 1) % numNodes;

        UdpEchoServerHelper echoServer(port);
        ApplicationContainer serverApps = echoServer.Install(nodes.Get(j));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(duration));

        UdpEchoClientHelper echoClient(interfaces.GetAddress(j), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(maxPackets));
        echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
        echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

        ApplicationContainer clientApps = echoClient.Install(nodes.Get(i));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(duration));
    }

    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowMonitor = flowmonHelper.InstallAll();

    AnimationInterface anim("adhoc_wireless_network.xml");
    anim.EnablePacketMetadata(true);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(duration));
    Simulator::Run();

    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowStats> stats = flowMonitor->GetFlowStats();

    for (auto it = stats.begin(); it != stats.end(); ++it) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
        std::cout << "Flow ID: " << it->first << " Src Addr: " << t.sourceAddress << " Dst Addr: " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets: " << it->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << it->second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << it->second.lostPackets << std::endl;
        std::cout << "  Throughput: " << it->second.rxBytes * 8.0 / (it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000 << " Mbps" << std::endl;
        std::cout << "  Mean Delay: " << it->second.delaySum.GetSeconds() / it->second.rxPackets << " s" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}