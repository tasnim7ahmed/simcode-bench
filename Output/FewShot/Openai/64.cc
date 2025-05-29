#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpStar");

void
PacketReceivedTrace(Ptr<const Packet> packet, const Address &address)
{
    static std::ofstream recvTrace("tcp-star-server.tr", std::ios_base::app);
    recvTrace << Simulator::Now().GetSeconds() << " PacketReceived " << packet->GetSize() << std::endl;
}

void
QueueDiscTrace(Ptr<const QueueDisc> qd)
{
    static std::ofstream queueTrace("tcp-star-server.tr", std::ios_base::app);
    uint32_t nPackets = qd->GetNPackets();
    queueTrace << Simulator::Now().GetSeconds() << " QueuePackets " << nPackets << std::endl;
}

int main(int argc, char *argv[])
{
    uint32_t numNodes = 9;
    uint32_t packetSize = 1024;
    std::string dataRate = "1Mbps";

    CommandLine cmd;
    cmd.AddValue("numNodes", "Number of nodes in the star (including hub)", numNodes);
    cmd.AddValue("packetSize", "CBR packet size (bytes)", packetSize);
    cmd.AddValue("dataRate", "CBR traffic rate (bps or e.g. 1Mbps)", dataRate);
    cmd.Parse(argc, argv);

    if (numNodes < 2) numNodes = 2;

    NodeContainer hubNode;
    hubNode.Create(1);

    NodeContainer armNodes;
    armNodes.Create(numNodes - 1);

    // star helper is not in core, so construct point-to-point star manually
    std::vector<NodeContainer> links;
    links.reserve(numNodes - 1);

    for (uint32_t i = 0; i < numNodes - 1; ++i)
    {
        NodeContainer link(hubNode.Get(0), armNodes.Get(i));
        links.push_back(link);
    }

    // Setup point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    p2p.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(100));

    std::vector<NetDeviceContainer> devices;
    for (uint32_t i = 0; i < numNodes - 1; ++i)
    {
        devices.push_back(p2p.Install(links[i]));
    }

    // Install TCP/IP
    InternetStackHelper stack;
    stack.Install(hubNode);
    stack.Install(armNodes);

    // Assign IP addresses
    std::vector<Ipv4InterfaceContainer> interfaces;
    char subnet[20];
    for (uint32_t i = 0; i < numNodes - 1; ++i)
    {
        std::snprintf(subnet, sizeof(subnet), "10.1.%u.0", i + 1);
        Ipv4AddressHelper address;
        address.SetBase(subnet, "255.255.255.0");
        interfaces.push_back(address.Assign(devices[i]));
    }

    // Install a PacketSink (TCP server) on the hub
    uint16_t port = 50000;
    Address hubAddress = InetSocketAddress(interfaces[0].GetAddress(0), port);

    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(hubNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(25.0));

    // Install OnOff apps (CBR TCP) on each arm node targeting the hub
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(interfaces[i].GetAddress(0), port));
        onoff.SetAttribute("DataRate", StringValue(dataRate));
        onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
        onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0 + i * 0.1)));
        onoff.SetAttribute("StopTime", TimeValue(Seconds(25.0)));
        clientApps.Add(onoff.Install(armNodes.Get(i)));
    }

    // Enable tracing: pcap per-device
    for (uint32_t i = 0; i < numNodes - 1; ++i)
    {
        for (uint32_t j = 0; j < 2; ++j)
        {
            std::ostringstream oss;
            oss << "tcp-star-server-" << devices[i].Get(j)->GetNode()->GetId() << "-" << devices[i].Get(j)->GetIfIndex() << ".pcap";
            p2p.EnablePcap(oss.str(), devices[i].Get(j), true, true);
        }
    }

    // Enable queue and packet sink tracing
    // Attach queue disc tracing to each device on the hub node
    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::PfifoFastQueueDisc");

    std::vector<Ptr<QueueDisc>> queueDiscs;
    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        // device at hub is index 0 in each link
        NetDeviceContainer &devs = devices[i];
        tch.Install(devs.Get(0));
        Ptr<PointToPointNetDevice> ptd = DynamicCast<PointToPointNetDevice>(devs.Get(0));
        Ptr<Queue<Packet>> queue = ptd->GetQueue();
        // Trace the queue depth at the device level, if queue exists
        if (queue) {
            queue->TraceConnectWithoutContext("PacketsInQueue",
                MakeCallback([](uint32_t oldValue, uint32_t newValue) {
                    static std::ofstream queueTrace("tcp-star-server.tr", std::ios_base::app);
                    queueTrace << Simulator::Now().GetSeconds() << " QueuePackets " << newValue << std::endl;
                }));
        }
    }

    // Packet sink tracing
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp.Get(0));
    if (sink) {
        sink->TraceConnectWithoutContext("Rx", MakeCallback(&PacketReceivedTrace));
    }

    Simulator::Stop(Seconds(25.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}