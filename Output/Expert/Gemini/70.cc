#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/queue.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    uint32_t nLeafNodes = 10;
    std::string queueDiscType = "RedQueueDisc";
    uint32_t maxPackets = 20;
    uint32_t mtu = 1500;
    DataRate bottleneckRate = DataRate("10Mbps");
    DataRate leafRate = DataRate("5Mbps");
    Time delay = MilliSeconds(2);
    double qDiscLimitP = 20;
    double qDiscLimitB = 60000;
    bool byteMode = false;
    uint32_t packetSize = 1024;
    double minTh = 5;
    double maxTh = 15;
    double maxP = 0.02;

    CommandLine cmd;
    cmd.AddValue("nLeafNodes", "Number of leaf nodes", nLeafNodes);
    cmd.AddValue("queueDiscType", "Queue disc type (RedQueueDisc or NlRedQueueDisc)", queueDiscType);
    cmd.AddValue("maxPackets", "Max Packets in Queue", maxPackets);
    cmd.AddValue("mtu", "MTU", mtu);
    cmd.AddValue("bottleneckRate", "Bottleneck Data Rate", bottleneckRate);
    cmd.AddValue("leafRate", "Leaf Data Rate", leafRate);
    cmd.AddValue("delay", "Delay", delay);
    cmd.AddValue("qDiscLimitP", "Queue Disc Limit Packets", qDiscLimitP);
    cmd.AddValue("qDiscLimitB", "Queue Disc Limit Bytes", qDiscLimitB);
    cmd.AddValue("byteMode", "Byte or Packet Mode", byteMode);
    cmd.AddValue("packetSize", "Packet Size", packetSize);
    cmd.AddValue("minTh", "Min Threshold", minTh);
    cmd.AddValue("maxTh", "Max Threshold", maxTh);
    cmd.AddValue("maxP", "Max Probability", maxP);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    NodeContainer leftRouters, rightRouters, leafNodes;
    leftRouters.Create(1);
    rightRouters.Create(1);
    leafNodes.Create(2 * nLeafNodes);

    PointToPointHelper bottleneckLink;
    bottleneckLink.SetDeviceAttribute("DataRate", DataRateValue(bottleneckRate));
    bottleneckLink.SetChannelAttribute("Delay", TimeValue(delay));

    PointToPointHelper leafLink;
    leafLink.SetDeviceAttribute("DataRate", DataRateValue(leafRate));
    leafLink.SetChannelAttribute("Delay", TimeValue(delay));

    NetDeviceContainer bottleneckDevices = bottleneckLink.Install(leftRouters.Get(0), rightRouters.Get(0));

    InternetStackHelper stack;
    stack.Install(leftRouters);
    stack.Install(rightRouters);
    stack.Install(leafNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer bottleneckInterfaces = address.Assign(bottleneckDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    NetDeviceContainer leftLeafDevices[nLeafNodes];
    NetDeviceContainer rightLeafDevices[nLeafNodes];
    Ipv4InterfaceContainer leftLeafInterfaces[nLeafNodes];
    Ipv4InterfaceContainer rightLeafInterfaces[nLeafNodes];

    for (uint32_t i = 0; i < nLeafNodes; ++i) {
        leftLeafDevices[i] = leafLink.Install(leftRouters.Get(0), leafNodes.Get(i));
        rightLeafDevices[i] = leafLink.Install(rightRouters.Get(0), leafNodes.Get(nLeafNodes + i));

        address.SetBase("10.1." + std::to_string(i + 2) + ".0", "255.255.255.0");
        leftLeafInterfaces[i] = address.Assign(leftLeafDevices[i]);
        address.SetBase("10.1." + std::to_string(i + 2 + nLeafNodes) + ".0", "255.255.255.0");
        rightLeafInterfaces[i] = address.Assign(rightLeafDevices[i]);
    }

    TypeId tid;
    if (queueDiscType == "RedQueueDisc") {
        tid = TypeId::LookupByName("ns3::RedQueueDisc");
    } else if (queueDiscType == "NlRedQueueDisc") {
        tid = TypeId::LookupByName("ns3::NlRedQueueDisc");
    } else {
        std::cerr << "Invalid queue disc type: " << queueDiscType << std::endl;
        return 1;
    }

    QueueDiscContainer queueDiscs;
    for (uint32_t i = 0; i < 1; ++i) {
        QueueDiscHelper queueDiscHelper(queueDiscType);
        queueDiscHelper.Set("LinkLayerAware", BooleanValue(false));
        queueDiscHelper.Set("LinkLayerModel", StringValue("ns3::PointToPointNetDevice"));
        queueDiscHelper.Set("EnablePrio", BooleanValue(false));
        queueDiscHelper.Set("EnableRED", BooleanValue(true));
        queueDiscHelper.Set("QueueLimit", switch (byteMode) {
                                            case false: return StringValue(std::to_string((uint32_t)qDiscLimitP) + "p");
                                            default: return StringValue(std::to_string((uint32_t)qDiscLimitB) + "b");
                                        });
        queueDiscHelper.Set("red:MinTh", DoubleValue(minTh));
        queueDiscHelper.Set("red:MaxTh", DoubleValue(maxTh));
        queueDiscHelper.Set("red:MaxP", DoubleValue(maxP));

        queueDiscs.Add(queueDiscHelper.Install(bottleneckDevices.Get(0)));
        queueDiscs.Add(queueDiscHelper.Install(bottleneckDevices.Get(1)));
    }

    for (uint32_t i = 0; i < nLeafNodes; ++i) {
        OnOffHelper onOffHelper("ns3::UdpSocketFactory", Address(InetSocketAddress(rightLeafInterfaces[i].GetAddress(1), 9)));
        onOffHelper.SetAttribute("OnTime", StringValue("ns3::UniformRandomVariable[Min=0.1|Max=0.2]"));
        onOffHelper.SetAttribute("OffTime", StringValue("ns3::UniformRandomVariable[Min=0.8|Max=1.2]"));
        onOffHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
        onOffHelper.SetAttribute("DataRate", DataRateValue(bottleneckRate / nLeafNodes));
        ApplicationContainer app = onOffHelper.Install(leafNodes.Get(i));
        app.Start(Seconds(1.0));
        app.Stop(Seconds(10.0));

        PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(rightLeafInterfaces[i].GetAddress(1), 9));
        ApplicationContainer sinkApp = sinkHelper.Install(leafNodes.Get(nLeafNodes + i));
        sinkApp.Start(Seconds(1.0));
        sinkApp.Stop(Seconds(10.0));
    }

    Simulator::Schedule(Seconds(10.0), [](){
        for (uint32_t i = 0; i < 2; ++i) {
            Ptr<QueueDisc> queueDisc = DynamicCast<QueueDisc>(bottleneckDevices.Get(i)->GetQueue());

            if(queueDisc){
                QueueDisc::UnsafeQueueDiscStats stats = queueDisc->GetStats();
                std::cout << "QueueDisc " << i << ": " << std::endl;
                std::cout << "  Drop Packets: " << stats.dropPackets << std::endl;
                std::cout << "  Drop Bytes: " << stats.dropBytes << std::endl;
                std::cout << "  Queued Packets: " << stats.queuedPackets << std::endl;
                std::cout << "  Queued Bytes: " << stats.queuedBytes << std::endl;
                std::cout << "  Dequeue Packets: " << stats.dequeuePackets << std::endl;
                std::cout << "  Dequeue Bytes: " << stats.dequeueBytes << std::endl;
                std::cout << "  Unacked Packets: " << stats.unackedPackets << std::endl;
                std::cout << "  Unacked Bytes: " << stats.unackedBytes << std::endl;
            } else {
                std::cerr << "Error: Queue Disc not found!" << std::endl;
            }

        }

    });

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}