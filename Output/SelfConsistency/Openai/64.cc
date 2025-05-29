#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"
#include "ns3/traffic-control-module.h"
#include <fstream>
#include <iomanip>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpStarTopology");

static void
QueueEnqueueTrace(std::ofstream *os, Ptr<const QueueItem> item)
{
    *os << Simulator::Now().GetSeconds() << " QueueEnqueue " << item->GetPacket()->GetUid()
        << std::endl;
}

static void
QueueDequeueTrace(std::ofstream *os, Ptr<const QueueItem> item)
{
    *os << Simulator::Now().GetSeconds() << " QueueDequeue " << item->GetPacket()->GetUid()
        << std::endl;
}

static void
RxTrace(std::ofstream *os, Ptr<const Packet> pkt, const Address &addr)
{
    *os << Simulator::Now().GetSeconds() << " PacketReceived " << pkt->GetUid()
        << " " << pkt->GetSize() << std::endl;
}

int
main(int argc, char *argv[])
{
    uint32_t nNodes = 9;
    std::string dataRate = "1Mbps";
    uint32_t packetSize = 512; // bytes

    CommandLine cmd(__FILE__);
    cmd.AddValue("nNodes", "Total number of star nodes (including the hub node)", nNodes);
    cmd.AddValue("dataRate", "Application data rate", dataRate);
    cmd.AddValue("packetSize", "CBR packet size in bytes", packetSize);
    cmd.Parse(argc, argv);

    if (nNodes < 2)
    {
        NS_LOG_ERROR("Need at least 2 nodes for a star topology!");
        return 1;
    }
    uint32_t nArms = nNodes - 1;

    NodeContainer allNodes;
    allNodes.Create(nNodes);

    // Index 0 is the hub, arms are nodes 1..nArms
    Ptr<Node> hubNode = allNodes.Get(0);

    InternetStackHelper stack;
    stack.Install(allNodes);

    // For storing net devices and interfaces
    std::vector<NetDeviceContainer> armNetDevices;
    std::vector<Ipv4InterfaceContainer> armInterfaces;

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    p2p.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(100));

    Ipv4AddressHelper address;
    std::string base = "10.1.1.0";
    uint32_t net = 1;

    // Set up links for each arm
    for (uint32_t i = 1; i <= nArms; ++i)
    {
        NodeContainer pair(hubNode, allNodes.Get(i));
        NetDeviceContainer devices = p2p.Install(pair);
        armNetDevices.push_back(devices);

        std::ostringstream subnet;
        subnet << "10.1." << net << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer iface = address.Assign(devices);
        armInterfaces.push_back(iface);

        // Enable pcap tracing with desired filename
        std::ostringstream pcapname;
        pcapname << "tcp-star-server-" << 0 << "-" << devices.Get(0)->GetIfIndex() << ".pcap";
        p2p.EnablePcap(pcapname.str(), devices.Get(0), true, true);

        ++net;
    }

    // Open trace file for queue/packet events
    std::ofstream traceFile("tcp-star-server.tr");

    // Install queue/packet traces on all side of hub
    for (size_t i = 0; i < armNetDevices.size(); ++i)
    {
        Ptr<PointToPointNetDevice> dev = DynamicCast<PointToPointNetDevice>(armNetDevices[i].Get(0));
        if (dev)
        {
            Ptr<Queue<Packet>> queue = dev->GetQueue();
            if (queue)
            {
                queue->TraceConnectWithoutContext(
                    "Enqueue", MakeBoundCallback(&QueueEnqueueTrace, &traceFile));
                queue->TraceConnectWithoutContext(
                    "Dequeue", MakeBoundCallback(&QueueDequeueTrace, &traceFile));
            }
        }
    }

    // Install applications: TCP PacketSink on the hub node, and OnOff TCP clients on each arm
    uint16_t port = 50000;
    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    // Each client talks to the hub using the per-arm subnet's hub-side address
    for (uint32_t i = 0; i < nArms; ++i)
    {
        // Setup TCP sink application at the hub for each arm
        Address sinkAddress(InetSocketAddress(armInterfaces[i].GetAddress(0), port + i));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
        ApplicationContainer sinkApp = sinkHelper.Install(hubNode);
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(10.0));

        serverApps.Add(sinkApp);

        // Connect a TCP OnOff application from arm node to the hub
        OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddress);
        onoff.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
        onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
        onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
        onoff.SetAttribute("StopTime", TimeValue(Seconds(9.5)));
        ApplicationContainer clientApp = onoff.Install(allNodes.Get(i + 1));
        clientApps.Add(clientApp);
    }

    // Attach reception trace callbacks
    for (uint32_t i = 0; i < serverApps.GetN(); ++i)
    {
        Ptr<Application> app = serverApps.Get(i);
        Ptr<PacketSink> sink = DynamicCast<PacketSink>(app);
        if (sink)
        {
            sink->TraceConnectWithoutContext("Rx", MakeBoundCallback(&RxTrace, &traceFile));
        }
    }

    // Enable IP forwarding on the hub if needed (not required in simple star, but for completeness)
    Ptr<Ipv4> ipv4 = hubNode->GetObject<Ipv4>();
    ipv4->SetAttribute("IpForward", BooleanValue(true));

    // Routing setup
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    traceFile.close();

    return 0;
}