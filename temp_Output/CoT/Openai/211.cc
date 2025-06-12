#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocWiFiSimulation");

int main(int argc, char *argv[])
{
    // Simulation parameters
    uint32_t numNodes = 5;
    double simTime = 20.0; // seconds
    uint32_t packetSize = 1024; // bytes
    std::string dataRate = "1Mbps";
    uint32_t numPacketsPerSecond = 100;

    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    // WiFi configuration
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiHelper wifiHelper;
    wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("DsssRate11Mbps"), "ControlMode", StringValue("DsssRate11Mbps"));

    NetDeviceContainer devices = wifiHelper.Install(wifiPhy, wifiMac, nodes);

    // Mobility - Random Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                              "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"),
                              "PositionAllocator", PointerValue(mobility.GetPositionAllocator()));
    mobility.Install(nodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP traffic: Node 0 -> Node 4
    uint16_t port = 5000;
    // Receiver at node 4
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(nodes.Get(4));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simTime));

    // Sender at node 0
    UdpClientHelper udpClient(interfaces.GetAddress(4), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(numPacketsPerSecond * (simTime - 2)));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0 / numPacketsPerSecond)));
    udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp = udpClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(simTime));

    // Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Analyze results
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::ofstream outFile;
    outFile.open("adhoc_wifi_results.txt");
    outFile << "SrcIP\tDstIP\tTxPackets\tRxPackets\tLostPackets\tThroughput (kbps)\n";
    for (const auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        double throughput = (flow.second.rxBytes * 8.0) / (simTime - 2.0) / 1024; // kbps
        outFile << t.sourceAddress << "\t" << t.destinationAddress << "\t"
                << flow.second.txPackets << "\t" << flow.second.rxPackets << "\t"
                << (flow.second.txPackets - flow.second.rxPackets) << "\t"
                << throughput << "\n";
    }
    outFile.close();

    Simulator::Destroy();
    return 0;
}