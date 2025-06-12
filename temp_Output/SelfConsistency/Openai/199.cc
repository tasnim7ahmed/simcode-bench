#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeRouterLanTcpExample");

int
main(int argc, char *argv[])
{
    // Enable logging if needed
    // LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Simulation parameters
    uint32_t nLanNodes = 3;
    double simTime = 10.0;
    std::string outputFileName = "tcp_3router_lan_results.txt";

    CommandLine cmd(__FILE__);
    cmd.AddValue("nLanNodes", "Number of LAN nodes (excluding R2)", nLanNodes);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("output", "Output results filename", outputFileName);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer lanNodes;
    lanNodes.Create(nLanNodes);
    Ptr<Node> r1 = CreateObject<Node>();
    Ptr<Node> r2 = CreateObject<Node>();
    Ptr<Node> r3 = CreateObject<Node>();
    NodeContainer routers = NodeContainer(r1, r2, r3);

    // Connect LAN to R2
    NodeContainer csmaNodes;
    csmaNodes.Add(lanNodes);
    csmaNodes.Add(r2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

    // Point-to-Point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // R1 <-> R2
    NodeContainer r1r2 = NodeContainer(r1, r2);
    NetDeviceContainer nd_r1r2 = p2p.Install(r1r2);

    // R2 <-> R3
    NodeContainer r2r3 = NodeContainer(r2, r3);
    NetDeviceContainer nd_r2r3 = p2p.Install(r2r3);

    // Install Internet Stack
    InternetStackHelper internet;
    internet.Install(lanNodes);
    internet.Install(routers);

    // Assign IP addresses
    Ipv4AddressHelper address;

    // LAN <-> R2
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if_lan = address.Assign(csmaDevices);

    // R1 <-> R2
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if_r1r2 = address.Assign(nd_r1r2);

    // R2 <-> R3
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer if_r2r3 = address.Assign(nd_r2r3);

    // Enable global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install TCP applications
    // Source: first LAN node
    Ptr<Node> tcpSource = lanNodes.Get(0);

    // Sink: node at R3 side
    Ptr<Node> tcpSink = r3;

    // Install PacketSink on R3
    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(if_r2r3.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApps = packetSinkHelper.Install(tcpSink);
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simTime + 1));

    // Install BulkSendApplication on LAN node
    BulkSendHelper sourceHelper("ns3::TcpSocketFactory", sinkAddress);
    sourceHelper.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited
    ApplicationContainer sourceApps = sourceHelper.Install(tcpSource);
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(simTime));

    // Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simTime + 1));
    Simulator::Run();

    // Output results
    std::ofstream out(outputFileName, std::ios::out);
    if (out.is_open())
    {
        monitor->CheckForLostPackets();
        Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
        std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

        out << "FlowID,SourceAddress,DestinationAddress,TxPackets,RxPackets,LostPackets,Throughput(bit/s)\n";
        for (const auto &flow : stats)
        {
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);

            double throughput = flow.second.rxBytes * 8.0 / (simTime * 1e6); // Mbps
            out << flow.first << ","
                << t.sourceAddress << ","
                << t.destinationAddress << ","
                << flow.second.txPackets << ","
                << flow.second.rxPackets << ","
                << (flow.second.txPackets - flow.second.rxPackets) << ","
                << throughput * 1e6 << "\n";
        }
        out.close();
    }
    else
    {
        NS_LOG_ERROR("Cannot open output file.");
    }

    Simulator::Destroy();
    return 0;
}