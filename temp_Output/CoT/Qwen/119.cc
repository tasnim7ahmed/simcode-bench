#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdHocWirelessNetwork");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 6;
    double duration = 10.0;
    uint32_t packetCount = 20;
    Time interPacketInterval = Seconds(0.5);
    std::string phyMode("DsssRate1Mbps");

    NodeContainer nodes;
    nodes.Create(numNodes);

    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

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

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    for (uint32_t i = 0; i < numNodes; ++i) {
        uint32_t serverNodeIndex = (i + 1) % numNodes;
        uint16_t port = 9;
        UdpEchoServerHelper echoServer(port);
        ApplicationContainer serverApps = echoServer.Install(nodes.Get(serverNodeIndex));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(duration));

        UdpEchoClientHelper echoClient(interfaces.GetAddress(serverNodeIndex), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(packetCount));
        echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps = echoClient.Install(nodes.Get(i));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(duration));
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AsciiTraceHelper asciiTraceHelper;
    wifiPhy.EnableAsciiAll(asciiTraceHelper.CreateFileStream("adhoc-wireless.tr"));

    AnimationInterface anim("adhoc-wireless.xml");

    Simulator::Stop(Seconds(duration));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = static_cast<Ipv4FlowClassifier*>(flowmon.GetClassifier().Peek())->FindFlow(iter->first);
        std::cout << "Flow ID: " << iter->first << " Src Addr: " << t.sourceAddress << " Dst Addr: " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets: " << iter->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << iter->second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << iter->second.lostPackets << std::endl;
        std::cout << "  PDR: " << ((double)iter->second.rxPackets / (double)iter->second.txPackets) * 100 << "%" << std::endl;
        std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps" << std::endl;
        std::cout << "  End-to-End Delay Mean: " << iter->second.delaySum.GetSeconds() / iter->second.rxPackets << " s" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}