#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/queue-disc-module.h"
#include <fstream>

using namespace ns3;

static void
PacketReceiveCallback(std::string context, Ptr<const Packet> packet, const Address &address)
{
    static std::ofstream rxLog("packet-receive.log", std::ios_base::app);
    rxLog << Simulator::Now().GetSeconds() << "s " << context << " PacketSize=" << packet->GetSize() << std::endl;
}

static void
QueueEnqueueCallback(std::string context, Ptr<const Packet> packet)
{
    static std::ofstream qLog("queue-activity.log", std::ios_base::app);
    qLog << Simulator::Now().GetSeconds() << "s " << context << " Enqueue PacketSize=" << packet->GetSize() << std::endl;
}

static void
QueueDequeueCallback(std::string context, Ptr<const Packet> packet)
{
    static std::ofstream qLog("queue-activity.log", std::ios_base::app);
    qLog << Simulator::Now().GetSeconds() << "s " << context << " Dequeue PacketSize=" << packet->GetSize() << std::endl;
}

static void
SetInterfaceDown(Ptr<Node> node, uint32_t interface)
{
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    ipv4->SetDown(interface);
    Ipv4GlobalRoutingHelper::RecomputeRoutingTables();
}

static void
SetInterfaceUp(Ptr<Node> node, uint32_t interface)
{
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    ipv4->SetUp(interface);
    Ipv4GlobalRoutingHelper::RecomputeRoutingTables();
}

int
main(int argc, char *argv[])
{
    // Enable logging for relevant components if desired
    // LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(7);

    // Naming nodes for code clarity
    Ptr<Node> n0 = nodes.Get(0);
    Ptr<Node> n1 = nodes.Get(1);
    Ptr<Node> n2 = nodes.Get(2);
    Ptr<Node> n3 = nodes.Get(3);
    Ptr<Node> n4 = nodes.Get(4);
    Ptr<Node> n5 = nodes.Get(5);
    Ptr<Node> n6 = nodes.Get(6);

    // Point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Mixed topology:
    // n1<--P2P1-->n2<--CSMA-->{n3,n4,n5}<--P2P2-->n6
    //      \--P2P3--n0

    // P2P1: n1<->n2
    NodeContainer p2p1(n1, n2);
    NetDeviceContainer dev_p2p1 = p2p.Install(p2p1);

    // P2P2: n5<->n6 (alternative point-to-point path to n6)
    NodeContainer p2p2(n5, n6);
    NetDeviceContainer dev_p2p2 = p2p.Install(p2p2);

    // P2P3: n1<->n0 (secondary link from n1)
    NodeContainer p2p3(n1, n0);
    NetDeviceContainer dev_p2p3 = p2p.Install(p2p3);

    // CSMA network: n2, n3, n4, n5
    NodeContainer csmaNodes;
    csmaNodes.Add(n2);
    csmaNodes.Add(n3);
    csmaNodes.Add(n4);
    csmaNodes.Add(n5);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer dev_csma = csma.Install(csmaNodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Address assignment
    Ipv4AddressHelper addr;

    addr.SetBase("10.1.1.0", "255.255.255.0"); // P2P1: n1<->n2
    Ipv4InterfaceContainer if_p2p1 = addr.Assign(dev_p2p1);

    addr.SetBase("10.1.2.0", "255.255.255.0"); // CSMA: n2, n3, n4, n5
    Ipv4InterfaceContainer if_csma = addr.Assign(dev_csma);

    addr.SetBase("10.1.3.0", "255.255.255.0"); // P2P2: n5<->n6
    Ipv4InterfaceContainer if_p2p2 = addr.Assign(dev_p2p2);

    addr.SetBase("10.1.4.0", "255.255.255.0"); // P2P3: n1<->n0
    Ipv4InterfaceContainer if_p2p3 = addr.Assign(dev_p2p3);

    // Enable global routing and interface event triggers
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Automatically recompute global routes upon interface changes
    Config::SetDefault("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue(true));

    // UDP Server on n6 (install packet sink listening on port 4000 and 4001)
    uint16_t port1 = 4000, port2 = 4001;
    UdpServerHelper server1(port1), server2(port2);
    ApplicationContainer serverApps1 = server1.Install(n6);
    serverApps1.Start(Seconds(0.5));
    serverApps1.Stop(Seconds(25.0));
    ApplicationContainer serverApps2 = server2.Install(n6);
    serverApps2.Start(Seconds(0.5));
    serverApps2.Stop(Seconds(25.0));

    // UDP/CBR client 1: node 1 -> node 6, port1, starts on p2p1 (primary path)
    UdpClientHelper client1(if_p2p2.GetAddress(1), port1);
    client1.SetAttribute("Interval", TimeValue(Seconds(0.05))); // 20 pkt/s = 0.05s
    client1.SetAttribute("MaxPackets", UintegerValue(200));
    client1.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp1 = client1.Install(n1);
    clientApp1.Start(Seconds(1.0));
    clientApp1.Stop(Seconds(11.0));

    // UDP/CBR client 2: node 1 -> node 6, port2, starts via alternative path after primary is down
    UdpClientHelper client2(if_p2p2.GetAddress(1), port2);
    client2.SetAttribute("Interval", TimeValue(Seconds(0.05))); // 20 pkt/s
    client2.SetAttribute("MaxPackets", UintegerValue(200));
    client2.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp2 = client2.Install(n1);
    clientApp2.Start(Seconds(11.0));
    clientApp2.Stop(Seconds(21.0));

    // Trace packet receptions on server
    Config::Connect("/NodeList/6/ApplicationList/*/$ns3::UdpServer/Rx", MakeCallback(&PacketReceiveCallback));

    // Trace queue activity for each NetDevice's queue
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Node> node = nodes.Get(i);
        for (uint32_t j = 0; j < node->GetNDevices(); ++j) {
            Ptr<NetDevice> dev = node->GetDevice(j);
            Ptr<PointToPointNetDevice> ptp = DynamicCast<PointToPointNetDevice>(dev);
            if (ptp) {
                Ptr<Queue<Packet> > queue = ptp->GetQueue();
                if (!queue)
                    continue;
                queue->TraceConnect("Enqueue", "NodeList/" + std::to_string(i) + "/DeviceList/" + std::to_string(j) + "/Enqueue", MakeCallback(&QueueEnqueueCallback));
                queue->TraceConnect("Dequeue", "NodeList/" + std::to_string(i) + "/DeviceList/" + std::to_string(j) + "/Dequeue", MakeCallback(&QueueDequeueCallback));
            }
            Ptr<CsmaNetDevice> csmaDev = DynamicCast<CsmaNetDevice>(dev);
            if (csmaDev) {
                Ptr<Queue<Packet> > queue = csmaDev->GetQueue();
                if (!queue)
                    continue;
                queue->TraceConnect("Enqueue", "NodeList/" + std::to_string(i) + "/DeviceList/" + std::to_string(j) + "/Enqueue", MakeCallback(&QueueEnqueueCallback));
                queue->TraceConnect("Dequeue", "NodeList/" + std::to_string(i) + "/DeviceList/" + std::to_string(j) + "/Dequeue", MakeCallback(&QueueDequeueCallback));
            }
        }
    }

    // Simulate interface down and up events to force rerouting
    // Down interface n2<->n5 (CSMA): disconnect n5's CSMA at 11s
    Simulator::Schedule(Seconds(11.0), &SetInterfaceDown, n2, 2); // n2's CSMA, assuming if 0:local, 1:p2p, 2:csma
    Simulator::Schedule(Seconds(11.0), &SetInterfaceDown, n5, 1); // n5's CSMA

    // At 19s, bring interfaces back up
    Simulator::Schedule(Seconds(19.0), &SetInterfaceUp, n2, 2);
    Simulator::Schedule(Seconds(19.0), &SetInterfaceUp, n5, 1);

    // Also simulate alt path via n0 by bringing down P2P1 at 13s, and up again at 17s
    Simulator::Schedule(Seconds(13.0), &SetInterfaceDown, n1, 1); // n1's p2p to n2 is likely if 1
    Simulator::Schedule(Seconds(13.0), &SetInterfaceDown, n2, 1); // n2's p2p to n1 is likely if 1
    Simulator::Schedule(Seconds(17.0), &SetInterfaceUp, n1, 1);
    Simulator::Schedule(Seconds(17.0), &SetInterfaceUp, n2, 1);

    // Run simulation
    Simulator::Stop(Seconds(22.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}