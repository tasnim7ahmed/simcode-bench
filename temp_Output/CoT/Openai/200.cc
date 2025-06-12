#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocAodvSimulation");

int main(int argc, char *argv[])
{
    uint32_t numNodes = 4;
    double simTime = 30.0;
    double areaSize = 100.0;
    std::string dataRate = "2Mbps";
    uint32_t packetSize = 512;
    double nodeSpeed = 5.0;
    uint32_t numFlows = 2;

    CommandLine cmd;
    cmd.AddValue("numNodes", "Number of ad hoc nodes", numNodes);
    cmd.AddValue("simTime", "Simulation Duration (seconds)", simTime);
    cmd.AddValue("areaSize", "Size of area (meters)", areaSize);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    WifiMacHelper wifiMac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("DsssRate11Mbps"));

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                              "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"),
                              "PositionAllocator", PointerValue(
                                    CreateObjectWithAttributes<RandomRectanglePositionAllocator>(
                                        "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                        "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"))));
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(20.0),
                                 "DeltaY", DoubleValue(20.0),
                                 "GridWidth", UintegerValue(2),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.Install(nodes);

    InternetStackHelper internet;
    AodvHelper aodv;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    uint16_t port = 9;
    ApplicationContainer apps;

    for (uint32_t i = 0; i < numFlows; ++i)
    {
        uint32_t srcNode = i;
        uint32_t dstNode = (i + 1) % numNodes;
        UdpServerHelper server(port);
        apps.Add(server.Install(nodes.Get(dstNode)));
        UdpClientHelper client(interfaces.GetAddress(dstNode), port);
        client.SetAttribute("MaxPackets", UintegerValue(10000));
        client.SetAttribute("Interval", TimeValue(Seconds(0.05)));
        client.SetAttribute("PacketSize", UintegerValue(packetSize));
        apps.Add(client.Install(nodes.Get(srcNode)));
    }

    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(simTime - 1));

    AnimationInterface anim("adhoc_aodv_netanim.xml");
    for (uint32_t i = 0; i < numNodes; i++)
    {
        anim.UpdateNodeDescription(i, "Node" + std::to_string(i));
        anim.UpdateNodeColor(i, 0, 255, 0); // Green
    }
    anim.SetMaxPktsPerTraceFile(1000000);

    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    double rxPackets = 0;
    double txPackets = 0;
    double throughput = 0;
    double sumDelay = 0;
    double avgDelay = 0;

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        txPackets += flow.second.txPackets;
        rxPackets += flow.second.rxPackets;
        throughput += flow.second.rxBytes * 8.0 / (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024;
        if (flow.second.rxPackets > 0)
        {
            sumDelay += flow.second.delaySum.GetSeconds();
        }
    }
    double pdr = (txPackets > 0) ? rxPackets / txPackets : 0;
    avgDelay = (rxPackets > 0) ? sumDelay / rxPackets : 0;

    std::cout << "Simulation Results:" << std::endl;
    std::cout << "Packet Delivery Ratio: " << pdr * 100 << "%" << std::endl;
    std::cout << "Average Throughput: " << throughput << " Mbps" << std::endl;
    std::cout << "Average End-to-End Delay: " << avgDelay * 1000 << " ms" << std::endl;

    Simulator::Destroy();
    return 0;
}