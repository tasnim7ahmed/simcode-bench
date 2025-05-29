#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/queue.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedPtPAndCsmaCdExample");

// Trace function for packet receptions
void RxTrace(Ptr<const Packet> packet, const Address &address)
{
    static std::ofstream rxStream("packet-receptions.txt", std::ios_base::app);
    rxStream << "At " << Simulator::Now().GetSeconds() << "s: Received packet of size "
             << packet->GetSize() << " bytes from " << InetSocketAddress::ConvertFrom(address).GetIpv4() << std::endl;
}

int main(int argc, char *argv[])
{
    // Enable logging for applications (optional)
    // LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create 7 nodes: n0..n6
    NodeContainer nodes;
    nodes.Create(7);

    // Names for easier reference
    Ptr<Node> n0 = nodes.Get(0), n1 = nodes.Get(1), n2 = nodes.Get(2);
    Ptr<Node> n3 = nodes.Get(3), n4 = nodes.Get(4), n5 = nodes.Get(5), n6 = nodes.Get(6);

    // Containers for each link/segment
    NodeContainer n0n2(n0, n2);
    NodeContainer n1n2(n1, n2);
    NodeContainer csmaNodes;
    csmaNodes.Add(n2);
    csmaNodes.Add(n3);
    csmaNodes.Add(n4);
    csmaNodes.Add(n5);
    NodeContainer n5n6(n5, n6);

    // Point-to-point helpers
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer d_n0n2 = p2p.Install(n0n2);
    NetDeviceContainer d_n1n2 = p2p.Install(n1n2);
    NetDeviceContainer d_n5n6 = p2p.Install(n5n6);

    // CSMA/CD segment
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("10Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
    NetDeviceContainer d_csma = csma.Install(csmaNodes);

    // Install protocol stack
    InternetStackHelper stack;
    stack.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper ipv4;

    // n0-n2: 10.1.1.0/24
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if_n0n2 = ipv4.Assign(d_n0n2);

    // n1-n2: 10.1.2.0/24
    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if_n1n2 = ipv4.Assign(d_n1n2);

    // n2, n3, n4, n5 CSMA: 10.1.3.0/24
    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer if_csma = ipv4.Assign(d_csma);

    // n5-n6: 10.1.4.0/24
    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer if_n5n6 = ipv4.Assign(d_n5n6);

    // Enable global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up the OnOff CBR/UDP flow from n0 to n6
    uint16_t port = 50000;
    Address sinkAddress(InetSocketAddress(if_n5n6.GetAddress(1), port)); // n6

    // Create PacketSink on n6
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(n6);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(20.0));

    // Connect packet reception trace
    Ptr<Application> sinkPtr = sinkApp.Get(0);
    sinkPtr->TraceConnectWithoutContext("Rx", MakeCallback(&RxTrace));

    // Install OnOff application on n0
    OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
    onoff.SetConstantRate(DataRate("300bps"), 50); // 50 bytes per packet, 300 bps data rate
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(19.0)));

    ApplicationContainer clientApps = onoff.Install(n0);

    // Enable pcap tracing for all devices
    p2p.EnablePcapAll("mixed-net-p2p");
    csma.EnablePcapAll("mixed-net-csma");

    // Trace queues for all p2p and csma devices
    Ptr<Queue<Packet>> queue;

    // n0n2 queue
    queue = d_n0n2.Get(0)->GetObject<PointToPointNetDevice>()->GetQueue();
    if (queue)
        queue->TraceConnectWithoutContext("Enqueue", MakeBoundCallback([](Ptr<const Packet> p) {
            static std::ofstream f("queue-trace.txt", std::ios_base::app);
            f << "Enqueue at " << Simulator::Now().GetSeconds() 
              << "s, size: " << p->GetSize() << " bytes\n";
        }));

    queue = d_n0n2.Get(0)->GetObject<PointToPointNetDevice>()->GetQueue();
    if (queue)
        queue->TraceConnectWithoutContext("Dequeue", MakeBoundCallback([](Ptr<const Packet> p) {
            static std::ofstream f("queue-trace.txt", std::ios_base::app);
            f << "Dequeue at " << Simulator::Now().GetSeconds() 
              << "s, size: " << p->GetSize() << " bytes\n";
        }));

    // Similar tracing for other p2p and csma devices
    for (uint32_t i = 0; i < d_n0n2.GetN(); ++i)
    {
        Ptr<NetDevice> dev = d_n0n2.Get(i);
        Ptr<PointToPointNetDevice> p2pDev = dev->GetObject<PointToPointNetDevice>();
        if (p2pDev && p2pDev->GetQueue())
        {
            p2pDev->GetQueue()->TraceConnectWithoutContext("Enqueue", MakeBoundCallback([](Ptr<const Packet> p) {
                static std::ofstream f("queue-trace.txt", std::ios_base::app);
                f << "P2P Enqueue at " << Simulator::Now().GetSeconds() 
                  << "s, size: " << p->GetSize() << " bytes\n";
            }));
            p2pDev->GetQueue()->TraceConnectWithoutContext("Dequeue", MakeBoundCallback([](Ptr<const Packet> p) {
                static std::ofstream f("queue-trace.txt", std::ios_base::app);
                f << "P2P Dequeue at " << Simulator::Now().GetSeconds()
                  << "s, size: " << p->GetSize() << " bytes\n";
            }));
        }
    }
    for (uint32_t i = 0; i < d_n1n2.GetN(); ++i)
    {
        Ptr<NetDevice> dev = d_n1n2.Get(i);
        Ptr<PointToPointNetDevice> p2pDev = dev->GetObject<PointToPointNetDevice>();
        if (p2pDev && p2pDev->GetQueue())
        {
            p2pDev->GetQueue()->TraceConnectWithoutContext("Enqueue", MakeBoundCallback([](Ptr<const Packet> p) {
                static std::ofstream f("queue-trace.txt", std::ios_base::app);
                f << "P2P Enqueue at " << Simulator::Now().GetSeconds() 
                  << "s, size: " << p->GetSize() << " bytes\n";
            }));
            p2pDev->GetQueue()->TraceConnectWithoutContext("Dequeue", MakeBoundCallback([](Ptr<const Packet> p) {
                static std::ofstream f("queue-trace.txt", std::ios_base::app);
                f << "P2P Dequeue at " << Simulator::Now().GetSeconds()
                  << "s, size: " << p->GetSize() << " bytes\n";
            }));
        }
    }
    for (uint32_t i = 0; i < d_n5n6.GetN(); ++i)
    {
        Ptr<NetDevice> dev = d_n5n6.Get(i);
        Ptr<PointToPointNetDevice> p2pDev = dev->GetObject<PointToPointNetDevice>();
        if (p2pDev && p2pDev->GetQueue())
        {
            p2pDev->GetQueue()->TraceConnectWithoutContext("Enqueue", MakeBoundCallback([](Ptr<const Packet> p) {
                static std::ofstream f("queue-trace.txt", std::ios_base::app);
                f << "P2P Enqueue at " << Simulator::Now().GetSeconds() 
                  << "s, size: " << p->GetSize() << " bytes\n";
            }));
            p2pDev->GetQueue()->TraceConnectWithoutContext("Dequeue", MakeBoundCallback([](Ptr<const Packet> p) {
                static std::ofstream f("queue-trace.txt", std::ios_base::app);
                f << "P2P Dequeue at " << Simulator::Now().GetSeconds()
                  << "s, size: " << p->GetSize() << " bytes\n";
            }));
        }
    }
    for (uint32_t i = 0; i < d_csma.GetN(); ++i)
    {
        Ptr<NetDevice> dev = d_csma.Get(i);
        Ptr<CsmaNetDevice> csmaDev = dev->GetObject<CsmaNetDevice>();
        if (csmaDev && csmaDev->GetQueue())
        {
            csmaDev->GetQueue()->TraceConnectWithoutContext("Enqueue", MakeBoundCallback([](Ptr<const Packet> p) {
                static std::ofstream f("queue-trace.txt", std::ios_base::app);
                f << "CSMA Enqueue at " << Simulator::Now().GetSeconds() 
                  << "s, size: " << p->GetSize() << " bytes\n";
            }));
            csmaDev->GetQueue()->TraceConnectWithoutContext("Dequeue", MakeBoundCallback([](Ptr<const Packet> p) {
                static std::ofstream f("queue-trace.txt", std::ios_base::app);
                f << "CSMA Dequeue at " << Simulator::Now().GetSeconds()
                  << "s, size: " << p->GetSize() << " bytes\n";
            }));
        }
    }

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}