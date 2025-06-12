#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/queue-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/stats-module.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellNetwork");

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("DumbbellNetwork", LOG_LEVEL_INFO);

    // Command line arguments
    uint32_t nLeafNodes = 1;
    std::string queueType = "RED"; // or "NLRED"
    std::string queueDiscLimit = "100p"; // packets or bytes
    uint32_t packetSize = 1024;
    std::string dataRate = "10Mbps";
    std::string delay = "2ms";
    uint32_t redMinTh = 20;
    uint32_t redMaxTh = 60;
    double redQW = 0.002;
    double redLInterm = 0.1;
    bool byteMode = false;

    CommandLine cmd;
    cmd.AddValue("nLeafNodes", "Number of leaf nodes", nLeafNodes);
    cmd.AddValue("queueType", "Queue type (RED or NLRED)", queueType);
    cmd.AddValue("queueDiscLimit", "Queue disc limit (e.g., 100p or 100b)", queueDiscLimit);
    cmd.AddValue("packetSize", "Packet size", packetSize);
    cmd.AddValue("dataRate", "Data rate", dataRate);
    cmd.AddValue("delay", "Delay", delay);
    cmd.AddValue("redMinTh", "RED min threshold", redMinTh);
    cmd.AddValue("redMaxTh", "RED max threshold", redMaxTh);
    cmd.AddValue("redQW", "RED queue weight", redQW);
    cmd.AddValue("redLInterm", "RED link idle time", redLInterm);
    cmd.AddValue("byteMode", "Enable byte mode (true or false)", byteMode);
    cmd.Parse(argc, argv);

    NS_LOG_INFO("Number of leaf nodes: " << nLeafNodes);
    NS_LOG_INFO("Queue type: " << queueType);
    NS_LOG_INFO("Queue disc limit: " << queueDiscLimit);
    NS_LOG_INFO("Packet size: " << packetSize);
    NS_LOG_INFO("Data rate: " << dataRate);
    NS_LOG_INFO("Delay: " << delay);
    NS_LOG_INFO("RED min threshold: " << redMinTh);
    NS_LOG_INFO("RED max threshold: " << redMaxTh);
    NS_LOG_INFO("RED queue weight: " << redQW);
    NS_LOG_INFO("RED link idle time: " << redLInterm);
    NS_LOG_INFO("Byte mode: " << byteMode);


    // Create core nodes (left and right)
    NodeContainer coreNodes;
    coreNodes.Create(2);

    // Create leaf nodes
    NodeContainer leftLeafNodes;
    leftLeafNodes.Create(nLeafNodes);
    NodeContainer rightLeafNodes;
    rightLeafNodes.Create(nLeafNodes);

    // Create point-to-point helper
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(dataRate));
    pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(coreNodes);
    stack.Install(leftLeafNodes);
    stack.Install(rightLeafNodes);

    // Connect left leaf nodes to the left core node
    NetDeviceContainer leftLeafDevices[nLeafNodes];
    for (uint32_t i = 0; i < nLeafNodes; ++i) {
        NodeContainer leafAndCore;
        leafAndCore.Add(leftLeafNodes.Get(i));
        leafAndCore.Add(coreNodes.Get(0));
        leftLeafDevices[i] = pointToPoint.Install(leafAndCore);
    }

    // Connect right leaf nodes to the right core node
    NetDeviceContainer rightLeafDevices[nLeafNodes];
    for (uint32_t i = 0; i < nLeafNodes; ++i) {
        NodeContainer leafAndCore;
        leafAndCore.Add(rightLeafNodes.Get(i));
        leafAndCore.Add(coreNodes.Get(1));
        rightLeafDevices[i] = pointToPoint.Install(leafAndCore);
    }

    // Connect the two core nodes
    NetDeviceContainer coreDevices = pointToPoint.Install(coreNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;

    for (uint32_t i = 0; i < nLeafNodes; ++i) {
        interfaces = address.Assign(leftLeafDevices[i]);
        address.NewNetwork();
    }
    interfaces = address.Assign(coreDevices);
    address.NewNetwork();

    for (uint32_t i = 0; i < nLeafNodes; ++i) {
        interfaces = address.Assign(rightLeafDevices[i]);
        address.NewNetwork();
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Configure queue disc
    TypeId queueTypeId;
    if (queueType == "RED") {
        queueTypeId = TypeId::LookupByName("ns3::RedQueueDisc");
    } else if (queueType == "NLRED") {
        queueTypeId = TypeId::LookupByName("ns3::NlredQueueDisc");
    } else {
        std::cerr << "Invalid queue type: " << queueType << std::endl;
        return 1;
    }

    QueueDiscContainer queueDiscs;
    QueueDiscHelper queueDiscHelper(queueType);
    queueDiscHelper.Set("LinkBandwidth", StringValue(dataRate));
    queueDiscHelper.Set("LinkDelay", StringValue(delay));
    queueDiscHelper.Set("queueLimit", StringValue(queueDiscLimit));
    if (queueType == "RED" || queueType == "NLRED") {
        queueDiscHelper.Set("MeanPktSize", UintegerValue(packetSize));
        queueDiscHelper.Set("红色最小值", DoubleValue(redMinTh));
        queueDiscHelper.Set("红色最大值", DoubleValue(redMaxTh));
        queueDiscHelper.Set("红色权重", DoubleValue(redQW));
        queueDiscHelper.Set("红色限制时间", DoubleValue(redLInterm));

        if (byteMode) {
            queueDiscHelper.Set("红色允许字节模式", BooleanValue(true));
        } else {
            queueDiscHelper.Set("红色允许字节模式", BooleanValue(false));
        }
    }

    queueDiscs.Add(queueDiscHelper.Install(coreDevices.Get(0)));
    queueDiscs.Add(queueDiscHelper.Install(coreDevices.Get(1)));


    // Install OnOff application on right leaf nodes
    for (uint32_t i = 0; i < nLeafNodes; ++i) {
        OnOffHelper onOffHelper("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(2 * nLeafNodes + 1 + i), 9)));
        onOffHelper.SetAttribute("OnTime", StringValue("ns3::UniformRandomVariable[Min=0.1|Max=1.0]"));
        onOffHelper.SetAttribute("OffTime", StringValue("ns3::UniformRandomVariable[Min=0.1|Max=1.0]"));
        onOffHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
        onOffHelper.SetAttribute("DataRate", StringValue(dataRate));
        ApplicationContainer onOffApp = onOffHelper.Install(rightLeafNodes.Get(i));
        onOffApp.Start(Seconds(1.0));
        onOffApp.Stop(Seconds(10.0));
    }

    // Install Sink application on left leaf nodes
    for (uint32_t i = 0; i < nLeafNodes; ++i) {
        PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
        ApplicationContainer sinkApp = sinkHelper.Install(leftLeafNodes.Get(i));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(10.0));
    }

    // Define a callback for queue statistics
    Simulator::Schedule(Seconds(0.1), [&]() {
        for (uint32_t i = 0; i < 2; ++i) {
            Ptr<QueueDisc> queueDisc = queueDiscs.Get(i);
            if (queueDisc) {
                if (queueDisc->GetNBytesDropped() != 0 || queueDisc->GetNPktsDropped() != 0) {
                    NS_LOG_INFO("Queue " << i << ": Bytes Dropped = " << queueDisc->GetNBytesDropped() << ", Packets Dropped = " << queueDisc->GetNPktsDropped());
                }
            } else {
                NS_LOG_ERROR("Queue disc " << i << " is null.");
            }
        }
        Simulator::Schedule(Seconds(0.1), std::bind(Simulator::Schedule, Seconds(0.1), std::placeholders::_1));
    }, std::bind(Simulator::Schedule, Seconds(0.1), std::placeholders::_1));

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}