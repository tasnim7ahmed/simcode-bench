#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NetworkSimulation");

int main(int argc, char *argv[]) {
    // Ensure output directories exist
    system("mkdir -p results/cwndTraces results/queue-size.dat results/pcap results/queueTraces results/queueStats.txt");

    // Write configuration to config.txt
    std::ofstream configOut("results/config.txt");
    configOut << "Four-node series topology:\n";
    configOut << "  n0 <-> n1: 10 Mbps, 1 ms delay\n";
    configOut << "  n1 <-> n2: 1 Mbps, 10 ms delay\n";
    configOut << "  n2 <-> n3: 10 Mbps, 1 ms delay\n";
    configOut << "TCP flow from n0 to n3 using BulkSendApplication\n";
    configOut.close();

    Time::SetResolution(Time::NS);
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper p2p_n0n1;
    p2p_n0n1.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p_n0n1.SetChannelAttribute("Delay", StringValue("1ms"));

    PointToPointHelper p2p_n1n2;
    p2p_n1n2.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2p_n1n2.SetChannelAttribute("Delay", StringValue("10ms"));

    PointToPointHelper p2p_n2n3;
    p2p_n2n3.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p_n2n3.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer dev_n0n1 = p2p_n0n1.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer dev_n1n2 = p2p_n1n2.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer dev_n2n3 = p2p_n2n3.Install(nodes.Get(2), nodes.Get(3));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if_n0n1 = address.Assign(dev_n0n1);

    address.NewNetwork();
    Ipv4InterfaceContainer if_n1n2 = address.Assign(dev_n1n2);

    address.NewNetwork();
    Ipv4InterfaceContainer if_n2n3 = address.Assign(dev_n2n3);

    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(if_n2n3.GetAddress(1), sinkPort));

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(3));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", sinkAddress);
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer sourceApps = bulkSendHelper.Install(nodes.Get(0));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    AsciiTraceHelper asciiTraceHelper;
    p2p_n0n1.EnableAsciiAll(asciiTraceHelper.CreateFileStream("results/queue-size.dat"));
    p2p_n1n2.EnableAsciiAll(asciiTraceHelper.CreateFileStream("results/queue-size.dat"));
    p2p_n2n3.EnableAsciiAll(asciiTraceHelper.CreateFileStream("results/queue-size.dat"));

    p2p_n0n1.EnablePcapAll("results/pcap/n0n1");
    p2p_n1n2.EnablePcapAll("results/pcap/n1n2");
    p2p_n2n3.EnablePcapAll("results/pcap/n2n3");

    Ptr<PointToPointNetDevice> nd_n0n1 = DynamicCast<PointToPointNetDevice>(dev_n0n1.Get(1));
    if (nd_n0n1) {
        nd_n0n1->GetQueue()->TraceConnectWithoutContext("Drop", MakeCallback([](Ptr<const Packet> p) {
            static std::ofstream dropFile("results/queueTraces/n0n1-drop.dat", std::ios::out | std::ios::app);
            dropFile << Simulator::Now().GetSeconds() << " " << p->GetSize() << std::endl;
        }));
    }

    Ptr<PointToPointNetDevice> nd_n1n2 = DynamicCast<PointToPointNetDevice>(dev_n1n2.Get(0));
    if (nd_n1n2) {
        nd_n1n2->GetQueue()->TraceConnectWithoutContext("Drop", MakeCallback([](Ptr<const Packet> p) {
            static std::ofstream dropFile("results/queueTraces/n1n2-drop.dat", std::ios::out | std::ios::app);
            dropFile << Simulator::Now().GetSeconds() << " " << p->GetSize() << std::endl;
        }));
    }

    Ptr<PointToPointNetDevice> nd_n2n3 = DynamicCast<PointToPointNetDevice>(dev_n2n3.Get(0));
    if (nd_n2n3) {
        nd_n2n3->GetQueue()->TraceConnectWithoutContext("Drop", MakeCallback([](Ptr<const Packet> p) {
            static std::ofstream dropFile("results/queueTraces/n2n3-drop.dat", std::ios::out | std::ios::app);
            dropFile << Simulator::Now().GetSeconds() << " " << p->GetSize() << std::endl;
        }));
    }

    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::BulkSendApplication/CongestionWindow", MakeBoundCallback([](std::string path, uint32_t oldVal, uint32_t cwnd) {
        static std::map<std::string, std::ofstream*> cwndStreams;
        auto it = cwndStreams.find(path);
        if (it == cwndStreams.end()) {
            std::string filename = "results/cwndTraces/" + path.substr(path.rfind("/") + 1) + ".dat";
            cwndStreams[path] = new std::ofstream(filename);
        }
        *cwndStreams[path] << Simulator::Now().GetSeconds() << " " << cwnd << std::endl;
    }, _1, _2, _3));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::ofstream queueStats("results/queueStats.txt");
    for (auto iter : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter.first);
        queueStats << "Flow " << iter.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        queueStats << "  Tx Packets: " << iter.second.txPackets << "\n";
        queueStats << "  Rx Packets: " << iter.second.rxPackets << "\n";
        queueStats << "  Lost Packets: " << iter.second.lostPackets << "\n";
        queueStats << "  Throughput: " << iter.second.rxBytes * 8.0 / (iter.second.timeLastRxPacket.GetSeconds() - iter.second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
        queueStats << "  Delay: " << iter.second.delaySum.GetSeconds() / iter.second.rxPackets << " s\n";
        queueStats << "  Jitter: " << iter.second.jitterSum.GetSeconds() / (iter.second.rxPackets > 1 ? iter.second.rxPackets - 1 : 1) << " s\n\n";
    }

    for (auto& [k, v] : stats) {
        (void)k; // silence unused variable warning
        delete v.packetSizeMean;
        delete v.delayMean;
        delete v.jitterMean;
    }

    Simulator::Destroy();
    return 0;
}