#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeRouterTcpSim");

int main(int argc, char *argv[])
{
    // Set simulation time
    double simTime = 10.0;

    // Command-line argument parsing
    CommandLine cmd;
    cmd.AddValue("simTime", "Simulation duration (seconds)", simTime);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer routers;
    routers.Create(3); // R1, R2, R3

    NodeContainer lanNodes;
    lanNodes.Create(3); // Three LAN nodes

    // Connect LAN to Router R2
    NodeContainer lanNet;
    lanNet.Add(lanNodes);
    lanNet.Add(routers.Get(1)); // Connect R2

    // Point-to-point links between routers
    NodeContainer r1r2 = NodeContainer(routers.Get(0), routers.Get(1)); // R1--R2
    NodeContainer r2r3 = NodeContainer(routers.Get(1), routers.Get(2)); // R2--R3

    // Point-to-point attributes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer dev_r1r2 = p2p.Install(r1r2);
    NetDeviceContainer dev_r2r3 = p2p.Install(r2r3);

    // CSMA for LAN
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer dev_lan = csma.Install(lanNet);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(routers);
    stack.Install(lanNodes);

    // IP Address assignment
    Ipv4AddressHelper ipv4;

    // LAN subnet
    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if_lan = ipv4.Assign(dev_lan);

    // R1--R2 subnet
    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if_r1r2 = ipv4.Assign(dev_r1r2);

    // R2--R3 subnet
    ipv4.SetBase("10.0.3.0", "255.255.255.0");
    Ipv4InterfaceContainer if_r2r3 = ipv4.Assign(dev_r2r3);

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Select LAN source and remote receiver (connected to R3)
    Ptr<Node> srcNode = lanNodes.Get(0);
    Ptr<Node> dstNode = routers.Get(2);

    // Application port
    uint16_t sinkPort = 50000;

    // Install a sink on R3
    Address sinkAddress(InetSocketAddress(if_r2r3.GetAddress(1), sinkPort));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(dstNode);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simTime));

    // TCP client on LAN node
    OnOffHelper clientHelper("ns3::TcpSocketFactory", sinkAddress);
    clientHelper.SetAttribute("DataRate", StringValue("5Mbps"));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    clientHelper.SetAttribute("StopTime", TimeValue(Seconds(simTime)));

    ApplicationContainer clientApp = clientHelper.Install(srcNode);

    // Enable pcap tracing
    p2p.EnablePcapAll("three-router-tcp");
    csma.EnablePcap("three-router-tcp-lan", dev_lan.Get(0), true);

    // Flow Monitor setup
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Gather statistics
    flowMonitor->CheckForLostPackets();
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    std::ofstream outFile;
    outFile.open("tcp-three-router-results.txt", std::ios::out | std::ios::trunc);
    outFile << "FlowId,SrcAddr,DestAddr,TxPackets,RxPackets,Throughput(bps)\n";

    for (const auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = flowHelper.GetClassifier()->FindFlow(flow.first);
        double throughput = flow.second.rxBytes * 8.0 / (simTime - 1.0); // Start at 1s
        outFile << flow.first << "," << t.sourceAddress << "," << t.destinationAddress << ","
                << flow.second.txPackets << "," << flow.second.rxPackets << ","
                << throughput << std::endl;
    }

    outFile.close();

    Simulator::Destroy();
    return 0;
}