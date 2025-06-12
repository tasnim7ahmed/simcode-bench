#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/queue.h"
#include <fstream>
#include <string>

using namespace ns3;

int main(int argc, char *argv[]) {
    std::string animFile = "series-network.xml";

    CommandLine cmd;
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpCubic"));

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper p2p0;
    p2p0.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p0.SetChannelAttribute("Delay", StringValue("1ms"));
    NetDeviceContainer d0 = p2p0.Install(nodes.Get(0), nodes.Get(1));

    PointToPointHelper p2p1;
    p2p1.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2p1.SetChannelAttribute("Delay", StringValue("10ms"));
    NetDeviceContainer d1 = p2p1.Install(nodes.Get(1), nodes.Get(2));

    PointToPointHelper p2p2;
    p2p2.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p2.SetChannelAttribute("Delay", StringValue("1ms"));
    NetDeviceContainer d2 = p2p2.Install(nodes.Get(2), nodes.Get(3));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0 = address.Assign(d0);
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1 = address.Assign(d1);
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i2 = address.Assign(d2);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 50000;
    BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(i2.GetAddress(1), port));
    source.SetAttribute("SendSize", UintegerValue(1400));
    source.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer sourceApps = source.Install(nodes.Get(0));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(nodes.Get(3));
    sinkApps.Start(Seconds(1.0));
    sinkApps.Stop(Seconds(10.0));

    std::ofstream configStream;
    configStream.open("results/config.txt");
    configStream << "DataRate (n0-n1): 10Mbps\n";
    configStream << "Delay (n0-n1): 1ms\n";
    configStream << "DataRate (n1-n2): 1Mbps\n";
    configStream << "Delay (n1-n2): 10ms\n";
    configStream << "DataRate (n2-n3): 10Mbps\n";
    configStream << "Delay (n2-n3): 1ms\n";
    configStream.close();

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));

    AnimationInterface anim(animFile);
    anim.SetConstantPosition(nodes.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(nodes.Get(1), 10.0, 0.0);
    anim.SetConstantPosition(nodes.Get(2), 20.0, 0.0);
    anim.SetConstantPosition(nodes.Get(3), 30.0, 0.0);

    std::string dirName = "results/pcap";
    std::string queueDirName = "results/queueTraces";
    std::string cwndDirName = "results/cwndTraces";
    std::string queueStatsFile = "results/queueStats.txt";

    std::system(("mkdir -p " + dirName).c_str());
    std::system(("mkdir -p " + queueDirName).c_str());
    std::system(("mkdir -p " + cwndDirName).c_str());

    p2p0.EnablePcapAll(dirName + "/series-network-0");
    p2p1.EnablePcapAll(dirName + "/series-network-1");
    p2p2.EnablePcapAll(dirName + "/series-network-2");

    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream(queueStatsFile);

    Simulator::Schedule(Seconds(0.1), [&]() {
        *stream->GetStream() << "Time,QueueSize,Drops\n";
    });

    for (int i = 0; i < 3; ++i) {
        std::string queueName = "DropTailQueue";
        Config::ConnectWithoutContext("/NodeList/" + std::to_string(i) + "/DeviceList/*/$" + queueName + "/Enqueue",
                                     MakeCallback(&Queue::TracedEnqueue));
        Config::ConnectWithoutContext("/NodeList/" + std::to_string(i) + "/DeviceList/*/$" + queueName + "/Dequeue",
                                     MakeCallback(&Queue::TracedDequeue));
        Config::ConnectWithoutContext("/NodeList/" + std::to_string(i) + "/DeviceList/*/$" + queueName + "/Drop",
                                     MakeCallback(&Queue::TracedDrop));

        Ptr<Queue> queue;
        if (i == 0) {
            queue = StaticCast<Queue>(d0.Get(1)->GetQueue());
        } else if (i == 1) {
            queue = StaticCast<Queue>(d1.Get(1)->GetQueue());
        } else {
            queue = StaticCast<Queue>(d2.Get(1)->GetQueue());
        }

        Simulator::Schedule(Seconds(0.1), [&, queue, i]() {
            *stream->GetStream() << Simulator::Now().GetSeconds() << "," << queue->GetNPackets() << "," << queue->GetNDroppedPackets() << "\n";
        });

    }

    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/*/CongestionWindow", MakeCallback(&CongestionWindow::CwndTracer));

    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::ofstream os;
    os.open("results/flowmon.txt");

    monitor->SerializeToStream(os, true, true);

    Simulator::Destroy();

    return 0;
}