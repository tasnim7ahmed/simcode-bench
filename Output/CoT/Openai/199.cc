#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeRoutersLanTcpSimulation");

int main(int argc, char *argv[])
{
    // Set simulation parameters
    double simTime = 10.0;

    // Enable logging for TCP applications/interests
    //LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    //LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create routers
    NodeContainer routers;
    routers.Create(3); // R1, R2, R3

    // Point-to-Point between R1-R2
    NodeContainer r1r2 = NodeContainer(routers.Get(0), routers.Get(1));
    // Point-to-Point between R2-R3
    NodeContainer r2r3 = NodeContainer(routers.Get(1), routers.Get(2));

    // LAN to R2 - 4 nodes + one router port for R2
    uint32_t nLan = 4;
    NodeContainer lanNodes;
    lanNodes.Create(nLan); // LAN hosts

    // LAN is: hosts + R2(routers.Get(1))
    NodeContainer lan;
    lan.Add(lanNodes);
    lan.Add(routers.Get(1)); // R2

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devR1R2 = p2p.Install(r1r2);
    NetDeviceContainer devR2R3 = p2p.Install(r2r3);

    // Create LAN (CSMA)
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer lanDevices = csma.Install(lan);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(routers);
    internet.Install(lanNodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaceR1R2 = ipv4.Assign(devR1R2);

    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaceR2R3 = ipv4.Assign(devR2R3);

    ipv4.SetBase("10.0.3.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaceLan = ipv4.Assign(lanDevices);

    // Set routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Allocate sender and receiver
    Ptr<Node> tcpSender = lanNodes.Get(0);
    Ptr<Node> tcpReceiver = routers.Get(2); // R3

    // Install TCP PacketSink on R3
    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(ifaceR2R3.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(tcpReceiver);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simTime + 1));

    // Install TCP BulkSend on one LAN node
    BulkSendHelper bulkSender("ns3::TcpSocketFactory", sinkAddress);
    bulkSender.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited
    ApplicationContainer senderApp = bulkSender.Install(tcpSender);
    senderApp.Start(Seconds(1.0));
    senderApp.Stop(Seconds(simTime));

    // Enable PCAP tracing (optional)
    p2p.EnablePcapAll("three-routers-lan-tcp");
    csma.EnablePcap("three-routers-lan-tcp-lan", lanDevices.Get(0), true);

    // FlowMonitor to gather statistics
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(simTime + 2));
    Simulator::Run();

    // Collect flow monitor statistics and save to file
    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::ofstream outFile;
    outFile.open("three-routers-lan-tcp-flowmon.csv");
    outFile << "FlowId,SrcAddr,DstAddr,TxPackets,RxPackets,LostPackets,Throughput(bps)\n";

    for (auto &iter : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter.first);
        double throughput = (iter.second.rxBytes * 8.0) / (simTime * 1000000.0); // Mbps
        outFile << iter.first << ","
                << t.sourceAddress << ","
                << t.destinationAddress << ","
                << iter.second.txPackets << ","
                << iter.second.rxPackets << ","
                << iter.second.lostPackets << ","
                << (iter.second.rxBytes * 8.0 / (iter.second.timeLastRxPacket.GetSeconds() - iter.second.timeFirstTxPacket.GetSeconds())) << "\n";
    }
    outFile.close();

    Simulator::Destroy();
    return 0;
}