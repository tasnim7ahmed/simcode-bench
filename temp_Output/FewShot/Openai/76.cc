#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-interface-container.h"
#include "ns3/packet-sink.h"
#include <fstream>

using namespace ns3;

void PacketReceptionCallback(Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet, const Address &address)
{
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\tReceived packet of size: " << packet->GetSize() << std::endl;
}

void QueueTraceCallback(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal)
{
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\tQueue size changed from " << oldVal << " to " << newVal << std::endl;
}

int main(int argc, char *argv[])
{
    // Create nodes: n0, n1, n2, n3, n4, n5, n6
    NodeContainer p2p_n0n2, p2p_n1n2, csma_nodes, p2p_n5n6;
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();
    Ptr<Node> n2 = CreateObject<Node>();
    Ptr<Node> n3 = CreateObject<Node>();
    Ptr<Node> n4 = CreateObject<Node>();
    Ptr<Node> n5 = CreateObject<Node>();
    Ptr<Node> n6 = CreateObject<Node>();

    // Point-to-point links
    p2p_n0n2.Add(n0);
    p2p_n0n2.Add(n2);

    p2p_n1n2.Add(n1);
    p2p_n1n2.Add(n2);

    p2p_n5n6.Add(n5);
    p2p_n5n6.Add(n6);

    // CSMA segment: n2,n3,n4,n5
    csma_nodes.Add(n2);
    csma_nodes.Add(n3);
    csma_nodes.Add(n4);
    csma_nodes.Add(n5);

    // Install Internet stack
    NodeContainer all_nodes;
    all_nodes.Add(n0);
    all_nodes.Add(n1);
    all_nodes.Add(n2);
    all_nodes.Add(n3);
    all_nodes.Add(n4);
    all_nodes.Add(n5);
    all_nodes.Add(n6);

    InternetStackHelper stack;
    stack.Install(all_nodes);

    // Point-to-point helpers
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Create p2p links
    NetDeviceContainer dev_n0n2 = p2p.Install(p2p_n0n2);
    NetDeviceContainer dev_n1n2 = p2p.Install(p2p_n1n2);
    NetDeviceContainer dev_n5n6 = p2p.Install(p2p_n5n6);

    // CSMA helper
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer csma_devs = csma.Install(csma_nodes);

    // Assign IP addresses
    Ipv4AddressHelper addr;
    Ipv4InterfaceContainer iface_n0n2, iface_n1n2, iface_csma, iface_n5n6;

    addr.SetBase("10.1.1.0", "255.255.255.0");
    iface_n0n2 = addr.Assign(dev_n0n2);

    addr.SetBase("10.1.2.0", "255.255.255.0");
    iface_n1n2 = addr.Assign(dev_n1n2);

    addr.SetBase("10.1.3.0", "255.255.255.0");
    iface_csma = addr.Assign(csma_devs);

    addr.SetBase("10.1.4.0", "255.255.255.0");
    iface_n5n6 = addr.Assign(dev_n5n6);

    // Enable PCAP on all devices
    p2p.EnablePcapAll("mixed-topology-p2p");
    csma.EnablePcapAll("mixed-topology-csma", false);

    // Sink app on n6
    uint16_t port = 9000;
    Address sinkAddress(InetSocketAddress(iface_n5n6.GetAddress(1), port));
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(n6);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(20.0));

    // OnOff app on n0 sending to n6
    OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
    onoff.SetAttribute("DataRate", StringValue("300bps"));
    onoff.SetAttribute("PacketSize", UintegerValue(50));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer clientApp = onoff.Install(n0);
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(20.0));

    // Trace reception at sink
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> rxStream = ascii.CreateFileStream("packet-receptions.tr");
    sinkApp.Get(0)->GetObject<PacketSink>()->TraceConnectWithoutContext("Rx", MakeBoundCallback(&PacketReceptionCallback, rxStream));

    // Trace queue sizes for all p2p devices
    Ptr<OutputStreamWrapper> queueStream = ascii.CreateFileStream("queue-trace.tr");
    for (uint32_t i = 0; i < dev_n0n2.GetN(); ++i)
    {
        Ptr<PointToPointNetDevice> dev = dev_n0n2.Get(i)->GetObject<PointToPointNetDevice>();
        Ptr<Queue<Packet>> queue = dev->GetQueue();
        if (queue)
        {
            queue->TraceConnectWithoutContext("PacketsInQueue", MakeBoundCallback(&QueueTraceCallback, queueStream));
        }
    }
    for (uint32_t i = 0; i < dev_n1n2.GetN(); ++i)
    {
        Ptr<PointToPointNetDevice> dev = dev_n1n2.Get(i)->GetObject<PointToPointNetDevice>();
        Ptr<Queue<Packet>> queue = dev->GetQueue();
        if (queue)
        {
            queue->TraceConnectWithoutContext("PacketsInQueue", MakeBoundCallback(&QueueTraceCallback, queueStream));
        }
    }
    for (uint32_t i = 0; i < dev_n5n6.GetN(); ++i)
    {
        Ptr<PointToPointNetDevice> dev = dev_n5n6.Get(i)->GetObject<PointToPointNetDevice>();
        Ptr<Queue<Packet>> queue = dev->GetQueue();
        if (queue)
        {
            queue->TraceConnectWithoutContext("PacketsInQueue", MakeBoundCallback(&QueueTraceCallback, queueStream));
        }
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}