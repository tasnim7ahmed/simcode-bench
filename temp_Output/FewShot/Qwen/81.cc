#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set default TCP configuration
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));

    // Enable logging for RED queue disc
    LogComponentEnable("RedQueueDisc", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Setup point-to-point link with TrafficControlHelper and RedQueueDisc
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));

    // Install RedQueueDisc using TrafficControlHelper
    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::RedQueueDisc",
                         "MinTh", DoubleValue(5),
                         "MaxTh", DoubleValue(15),
                         "LinkBandwidth", DataRateValue(DataRate("1Mbps")),
                         "LinkDelay", TimeValue(MilliSeconds(10)));
    NetDeviceContainer devices = p2p.Install(nodes);
    tch.Install(devices);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup TCP applications
    uint16_t port = 5000;
    BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    source.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited data

    ApplicationContainer senderApp = source.Install(nodes.Get(0));
    senderApp.Start(Seconds(1.0));
    senderApp.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer receiverApp = sink.Install(nodes.Get(1));
    receiverApp.Start(Seconds(1.0));
    receiverApp.Stop(Seconds(10.0));

    // FlowMonitor setup to collect statistics
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Callback to track queue size at device layer
    auto devQueueSizeTrace = [](Ptr<const NetDevice> dev, Ptr<const QueueItem> item) {
        std::cout << "Device Queue Size: " << dev->GetObject<PointToPointNetDevice>()->GetQueue()->GetNPackets() << " packets\n";
    };
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/TxQueue/Drop", MakeCallback(devQueueSizeTrace));

    // Callback to track queue size at traffic control layer
    auto tcQueueSizeTrace = [](Ptr<const QueueDisc> qd, Ptr<const QueueItem> item) {
        std::cout << "Traffic Control Queue Size: " << qd->GetInternalQueue(0)->GetNPackets() << " packets\n";
    };
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::TrafficControlLayer/RootQueueDisc/Object/$ns3::RedQueueDisc/Enqueue", MakeCallback(tcQueueSizeTrace));

    // Callback to track sojourn time (time in queue)
    auto sojournTimeTrace = [](Ptr<const QueueDiscItem> item, const Time& delay) {
        std::cout << "Packet Sojourn Time: " << delay.As(Time::MS) << "\n";
    };
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::TrafficControlLayer/RootQueueDisc/Object/$ns3::RedQueueDisc/SojournTime", MakeCallback(sojournTimeTrace));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Print flow statistics
    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = static_cast<Ipv4FlowClassifier*>(flowmon.GetClassifier().Get())->FindFlow(iter->first);
        std::cout << "Flow ID: " << iter->first << " Src Addr: " << t.sourceAddress << " Dst Addr: " << t.destinationAddress << "\n";
        std::cout << "  Tx Packets: " << iter->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << iter->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << iter->second.lostPackets << "\n";
        std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
        std::cout << "  Mean Delay: " << iter->second.delaySum.GetSeconds() / iter->second.rxPackets << " s\n";
        std::cout << "  Jitter: " << iter->second.jitterSum.GetSeconds() / (iter->second.rxPackets > 1 ? iter->second.rxPackets - 1 : 1) << " s\n";
    }

    Simulator::Destroy();
    return 0;
}