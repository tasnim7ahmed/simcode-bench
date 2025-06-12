#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdHocAodvSimulation");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 10;
    double simTime = 60.0;
    double echoInterval = 4.0;
    bool enablePcap = true;
    bool enableRoutingTablePrint = true;
    double routingTablePrintInterval = 10.0;
    std::string animFile = "ad_hoc_aodv.xml";

    CommandLine cmd(__FILE__);
    cmd.AddValue("numNodes", "Number of nodes in the grid.", numNodes);
    cmd.AddValue("simTime", "Total simulation time (seconds)", simTime);
    cmd.AddValue("echoInterval", "Echo request interval (seconds)", echoInterval);
    cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
    cmd.AddValue("enableRoutingTablePrint", "Enable periodic printing of routing tables", enableRoutingTablePrint);
    cmd.AddValue("routingTablePrintInterval", "Interval for routing table printing", routingTablePrintInterval);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(20.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(nodes);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < numNodes; ++i) {
        serverApps.Add(echoServer.Install(nodes.Get(i)));
    }
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    for (uint32_t i = 0; i < numNodes; ++i) {
        uint32_t nextNode = (i + 1) % numNodes;
        Ipv4Address nextAddr = interfaces.GetAddress(nextNode, 0);
        UdpEchoClientHelper echoClient(nextAddr, 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(UINT32_MAX));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(echoInterval)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));
        ApplicationContainer clientApp = echoClient.Install(nodes.Get(i));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(simTime - 1.0));
    }

    if (enableRoutingTablePrint) {
        aodv.PrintRoutingTableEvery(Seconds(routingTablePrintInterval), nodes, CreateObject<OutputStreamWrapper>("aodv-routing-tables.routes", std::ios::out));
    }

    if (enablePcap) {
        wifiPhy.EnablePcapAll("ad_hoc_aodv");
    }

    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

    AnimationInterface anim(animFile);
    for (uint32_t i = 0; i < numNodes; ++i) {
        anim.UpdateNodeDescription(nodes.Get(i), "Node-" + std::to_string(i));
        anim.UpdateNodeColor(nodes.Get(i), 255, 0, 0);
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    flowmon->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();

    std::ofstream outFile("ad_hoc_aodv_results.txt");
    outFile << "FlowId\tThroughput (Mbps)\tPacketDeliveryRatio\tEndToEndDelay\n";
    for (auto it = stats.begin(); it != stats.end(); ++it) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
        std::stringstream protoStream;
        protoStream << (uint32_t)t.protocol;
        if (t.protocol == 6) protoStream.str("TCP");
        else if (t.protocol == 17) protoStream.str("UDP");
        else if (t.protocol == 1) protoStream.str("ICMP");

        double packetDeliveryRatio = static_cast<double>(it->second.rxPackets) / (it->second.txPackets ? it->second.txPackets : 1);
        double throughput = (it->second.rxBytes * 8.0) / (it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds()) / 1e6;
        double delay = (it->second.delaySum.GetSeconds() / (it->second.rxPackets ? it->second.rxPackets : 1));

        outFile << it->first << "\t"
                << throughput << "\t"
                << packetDeliveryRatio << "\t"
                << delay << "\n";
    }
    outFile.close();

    Simulator::Destroy();
    return 0;
}