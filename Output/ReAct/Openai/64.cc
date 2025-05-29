#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpStarServer");

int main(int argc, char *argv[])
{
    // Default parameters
    uint32_t numNodes = 9; // 1 hub + 8 arms
    std::string dataRate = "1Mbps";
    uint32_t packetSize = 1024;

    // Command line args
    CommandLine cmd;
    cmd.AddValue("numNodes", "Total number of nodes (>=2)", numNodes);
    cmd.AddValue("packetSize", "CBR application packet size (bytes)", packetSize);
    cmd.AddValue("dataRate", "CBR application data rate (bps)", dataRate);
    cmd.Parse(argc, argv);

    if (numNodes < 2) numNodes = 2;
    uint32_t armNodes = numNodes - 1;

    NodeContainer hubNode;
    hubNode.Create(1);
    NodeContainer armNode;
    armNode.Create(armNodes);

    // Create links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    p2p.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(100));

    // Create net devices and containers to record device/interface numbers
    std::vector<NetDeviceContainer> deviceContainers(armNodes);
    std::vector<Ipv4InterfaceContainer> interfaceContainers(armNodes);

    // Stack installation
    InternetStackHelper stack;
    stack.Install(hubNode);
    stack.Install(armNode);

    // Assign addresses and connect nodes
    Ipv4AddressHelper address;
    for (uint32_t i = 0; i < armNodes; ++i)
    {
        NodeContainer pair(hubNode.Get(0), armNode.Get(i));
        deviceContainers[i] = p2p.Install(pair);
        std::ostringstream subnet;
        subnet << "10.1." << i+1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaceContainers[i] = address.Assign(deviceContainers[i]);
    }

    // Routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install PacketSink (TCP) on hub node for each connection
    uint16_t basePort = 50000;
    ApplicationContainer sinkApps;
    for (uint32_t i = 0; i < armNodes; ++i)
    {
        Address sinkAddress(InetSocketAddress(interfaceContainers[i].GetAddress(0), basePort + i));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
        ApplicationContainer sinkApp = sinkHelper.Install(hubNode.Get(0));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(20.0));
        sinkApps.Add(sinkApp);
    }

    // Install CBR TCP applications on arm nodes
    for (uint32_t i = 0; i < armNodes; ++i)
    {
        // TCP OnOffApplication
        OnOffHelper clientHelper("ns3::TcpSocketFactory",
            Address(InetSocketAddress(interfaceContainers[i].GetAddress(0), basePort + i)));
        clientHelper.SetAttribute("DataRate", StringValue(dataRate));
        clientHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
        clientHelper.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
        clientHelper.SetAttribute("StopTime", TimeValue(Seconds(20.0)));
        clientHelper.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited
        ApplicationContainer clientApps = clientHelper.Install(armNode.Get(i));
    }

    // Enable pcap tracing
    for (uint32_t i = 0; i < armNodes; ++i)
    {
        // Interface 0: hub, Interface 1: arm node
        std::ostringstream ossHub, ossArm;
        ossHub << "tcp-star-server-" << hubNode.Get(0)->GetId() << "-0.pcap";
        ossArm << "tcp-star-server-" << armNode.Get(i)->GetId() << "-1.pcap";
        deviceContainers[i].Get(0)->TraceConnect("PhyRxDrop", "", MakeCallback([](Ptr<const Packet>){ }));
        deviceContainers[i].Get(1)->TraceConnect("PhyRxDrop", "", MakeCallback([](Ptr<const Packet>){ }));
        p2p.EnablePcap(ossHub.str(), deviceContainers[i].Get(0), true, true);
        p2p.EnablePcap(ossArm.str(), deviceContainers[i].Get(1), true, true);
    }

    // Enable queue and packet reception tracing to tcp-star-server.tr
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("tcp-star-server.tr");
    for (uint32_t i = 0; i < armNodes; ++i)
    {
        // Queue events
        Ptr<PointToPointNetDevice> dev = deviceContainers[i].Get(0)->GetObject<PointToPointNetDevice>();
        Ptr<Queue<Packet>> queue = dev->GetQueue();
        if (queue)
        {
            queue->TraceConnectWithoutContext("Enqueue", MakeBoundCallback(
                [](Ptr<OutputStreamWrapper> stream, Ptr<const Packet> p) {
                    *stream->GetStream() << "Enqueue " << Simulator::Now().GetSeconds() << "\n";
                }, stream));
            queue->TraceConnectWithoutContext("Dequeue", MakeBoundCallback(
                [](Ptr<OutputStreamWrapper> stream, Ptr<const Packet> p) {
                    *stream->GetStream() << "Dequeue " << Simulator::Now().GetSeconds() << "\n";
                }, stream));
            queue->TraceConnectWithoutContext("Drop", MakeBoundCallback(
                [](Ptr<OutputStreamWrapper> stream, Ptr<const Packet> p) {
                    *stream->GetStream() << "Drop " << Simulator::Now().GetSeconds() << "\n";
                }, stream));
        }
        // Packet reception at PacketSink
        Ptr<Application> app = sinkApps.Get(i);
        Ptr<PacketSink> sink = DynamicCast<PacketSink>(app);
        if (sink)
        {
            sink->TraceConnectWithoutContext("Rx", MakeBoundCallback(
                [](Ptr<OutputStreamWrapper> stream, Ptr<const Packet> p, const Address &addr) {
                    *stream->GetStream() << "Rx " << Simulator::Now().GetSeconds() << "\n";
                }, stream));
        }
    }

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}