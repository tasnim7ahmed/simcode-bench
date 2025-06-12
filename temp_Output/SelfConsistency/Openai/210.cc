/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridTopologyTcpUdp");

static void
ReceivePacket (Ptr<const Packet> packet, const Address &src)
{
    // For tracing, can be extended as needed.
}

uint32_t udpPacketsSent = 0;
uint32_t udpPacketsReceived = 0;
double udpFirstSentTime = 0;
double udpLastReceivedTime = 0;

void UdpTxTrace (Ptr<const Packet> p)
{
    udpPacketsSent++;
    if (udpFirstSentTime == 0)
        udpFirstSentTime = Simulator::Now().GetSeconds();
}

void UdpRxTrace (Ptr<const Packet> p, const Address &a)
{
    udpPacketsReceived++;
    udpLastReceivedTime = Simulator::Now().GetSeconds();
}

int
main (int argc, char *argv[])
{
    uint32_t nStarClients = 4;
    uint32_t nMeshNodes = 5;
    uint32_t nUdpPairs = 6; // number of UDP flows
    double simulationTime = 15.0; // seconds

    CommandLine cmd;
    cmd.AddValue("nStarClients", "Number of clients in TCP star", nStarClients);
    cmd.AddValue("nMeshNodes", "Number of mesh UDP nodes", nMeshNodes);
    cmd.AddValue("simulationTime", "Simulation time (s)", simulationTime);
    cmd.Parse(argc, argv);

    // ===================== STAR TOPOLOGY (TCP) ===============================
    NodeContainer starClients;
    starClients.Create(nStarClients);
    NodeContainer starServer;
    starServer.Create(1);

    NodeContainer allStarNodes;
    allStarNodes.Add(starServer.Get(0));
    allStarNodes.Add(starClients);

    // Use point-to-point links between clients and server
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("20Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // For each star client, make a P2P link to starServer
    NetDeviceContainer serverDevs;
    NetDeviceContainer clientDevs[nStarClients];
    for (uint32_t i = 0; i < nStarClients; ++i)
    {
        NodeContainer pair(starClients.Get(i), starServer.Get(0));
        NetDeviceContainer link = p2p.Install(pair);
        clientDevs[i] = NetDeviceContainer(link.Get(0));
        serverDevs.Add(link.Get(1));
    }

    // ===================== MESH TOPOLOGY (UDP) ================================
    NodeContainer meshNodes;
    meshNodes.Create(nMeshNodes);

    // Use Wifi Adhoc for mesh
    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211g);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.Set ("TxPowerStart", DoubleValue (16.0));
    phy.Set ("TxPowerEnd", DoubleValue (16.0));
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel (channel.Create ());

    WifiMacHelper mac;
    mac.SetType ("ns3::AdhocWifiMac");

    NetDeviceContainer meshDevs = wifi.Install (phy, mac, meshNodes);

    // Mobility for mesh to arrange in a grid
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue (0.0),
                                   "MinY", DoubleValue (30.0),
                                   "DeltaX", DoubleValue (30.0),
                                   "DeltaY", DoubleValue (30.0),
                                   "GridWidth", UintegerValue (3),
                                   "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (meshNodes);

    // static positions for star
    MobilityHelper staticMobility;
    staticMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    staticMobility.Install (allStarNodes);
    // Assign star clients to positions spaced out, server in center
    Ptr<MobilityModel> mm;
    for (uint32_t i = 0; i < nStarClients; ++i)
    {
        mm = allStarNodes.Get(i+1)->GetObject<MobilityModel>();
        mm->SetPosition (Vector (25+30*i, 0.0, 0.0));
    }
    mm = allStarNodes.Get(0)->GetObject<MobilityModel>();
    mm->SetPosition (Vector (75, 15, 0.0));

    // ========================== INTERNET + IP ================================
    InternetStackHelper stack;
    stack.Install(allStarNodes);
    stack.Install(meshNodes);

    Ipv4AddressHelper ipv4;

    // Assign IP addresses for star links
    std::vector<Ipv4InterfaceContainer> clientInterfaces(nStarClients);
    for (uint32_t i = 0; i < nStarClients; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i+1 << ".0";
        ipv4.SetBase (Ipv4Address (subnet.str ().c_str ()), "255.255.255.0");
        clientInterfaces[i] = ipv4.Assign (NetDeviceContainer (clientDevs[i].Get(0), serverDevs.Get(i)));
    }

    // Assign IP addresses for mesh nodes
    ipv4.SetBase ("10.2.0.0", "255.255.255.0");
    Ipv4InterfaceContainer meshInterfaces = ipv4.Assign(meshDevs);

    // ======================== APPLICATIONS ===================================

    // ----- TCP Star: BulkSend from each client to server -----
    uint16_t tcpPort = 5000;
    ApplicationContainer tcpApps;
    for (uint32_t i = 0; i < nStarClients; ++i)
    {
        // Install TCP sink at server
        Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny(), tcpPort+i));
        PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
        ApplicationContainer sinkApp = packetSinkHelper.Install (starServer.Get(0));
        sinkApp.Start (Seconds (0.0));
        sinkApp.Stop (Seconds (simulationTime));
        tcpApps.Add(sinkApp);

        // Install BulkSend application on client to server
        BulkSendHelper bulkSender ("ns3::TcpSocketFactory",
                                   InetSocketAddress (clientInterfaces[i].GetAddress(1), tcpPort+i));
        bulkSender.SetAttribute ("MaxBytes", UintegerValue(0)); // unlimited
        ApplicationContainer sourceApp = bulkSender.Install (starClients.Get(i));
        sourceApp.Start (Seconds (1.0 + 0.2*i));
        sourceApp.Stop (Seconds (simulationTime));
        tcpApps.Add(sourceApp);
    }

    // ----- UDP Mesh: OnOffApplication pair-wise flows -----
    ApplicationContainer udpApps;
    uint16_t udpPortBase = 6000;

    // Schedule nUdpPairs random flows between mesh nodes, avoiding src==dst
    Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
    for (uint32_t i = 0; i < nUdpPairs; ++i)
    {
        uint32_t srcIdx, dstIdx;
        do {
            srcIdx = uv->GetInteger(0, nMeshNodes - 1);
            dstIdx = uv->GetInteger(0, nMeshNodes - 1);
        } while (srcIdx == dstIdx);

        // Install UDP sink at destination
        PacketSinkHelper udpSinkHelper ("ns3::UdpSocketFactory",
            InetSocketAddress (Ipv4Address::GetAny (), udpPortBase + i));
        ApplicationContainer udpSinkApp = udpSinkHelper.Install (meshNodes.Get(dstIdx));
        udpSinkApp.Start (Seconds (0.0));
        udpSinkApp.Stop (Seconds (simulationTime));
        udpApps.Add(udpSinkApp);

        // Install OnOff App at src to dst
        OnOffHelper onoff ("ns3::UdpSocketFactory",
                           InetSocketAddress (meshInterfaces.GetAddress(dstIdx), udpPortBase + i));
        onoff.SetAttribute ("DataRate", StringValue ("10Mbps"));
        onoff.SetAttribute ("PacketSize", UintegerValue (1024));
        onoff.SetAttribute ("StartTime", TimeValue(Seconds(2.0+ i*0.1)));
        onoff.SetAttribute ("StopTime", TimeValue(Seconds(simulationTime)));
        ApplicationContainer udpSrcApp = onoff.Install (meshNodes.Get(srcIdx));
        udpApps.Add(udpSrcApp);
    }

    // For simple UDP stats, tap into sinks (first UDP sink).
    Ptr<Application> udpSink = udpApps.Get(0);
    udpSink->GetObject<PacketSink>()->TraceConnectWithoutContext ("Rx", MakeCallback (&UdpRxTrace));
    // For UDP senders
    Ptr<Application> udpSource = udpApps.Get(nUdpPairs); // first OnOff
    udpSource->GetObject<OnOffApplication>()->TraceConnectWithoutContext ("Tx", MakeCallback (&UdpTxTrace));

    // ========================= FLOW MONITOR ==================================
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> monitor = flowHelper.InstallAll();

    // ========================= NETANIM SETUP =================================
    AnimationInterface anim ("hybrid-tcp-udp.xml");
    // For better coloring
    for (uint32_t i=0; i < nStarClients; ++i)
        anim.UpdateNodeColor (allStarNodes.Get(i+1), 0, 255, 0); // green - TCP clients
    anim.UpdateNodeColor (allStarNodes.Get(0), 255, 0, 0);      // red - TCP server
    for (uint32_t i=0; i<meshNodes.GetN(); ++i)
        anim.UpdateNodeColor (meshNodes.Get(i), 0, 0, 255);     // blue - mesh nodes

    // ======================= RUN SIMULATION ==================================
    Simulator::Stop (Seconds (simulationTime));
    Simulator::Run ();

    // ================= FLOW MONITOR RESULTS ===================================
    monitor->CheckForLostPackets ();
    double tcpThroughput = 0.0;
    double tcpDelay = 0.0;
    uint64_t tcpRxPackets = 0, tcpLostPackets = 0;

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowHelper.GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin (); iter != stats.end (); ++iter)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);
        // Print only TCP flows (star)
        if (t.destinationPort >= tcpPort && t.destinationPort < tcpPort + nStarClients)
        {
            double rxThroughput = iter->second.rxBytes * 8.0 / (simulationTime - 1.0) / 1000000.0; // Mbps
            double avgDelay = iter->second.delaySum.GetSeconds () / iter->second.rxPackets;
            tcpThroughput += rxThroughput;
            tcpDelay += avgDelay;
            tcpRxPackets += iter->second.rxPackets;
            tcpLostPackets += iter->second.lostPackets;
            std::cout << "TCP Flow: " << t.sourceAddress << " --> " << t.destinationAddress
                      << " Port: " << t.destinationPort
                      << " RxBytes: " << iter->second.rxBytes
                      << " Throughput: " << rxThroughput << " Mbps"
                      << " AvgDelay: " << avgDelay << " s"
                      << " LostPackets: " << iter->second.lostPackets
                      << std::endl;
        }
        // UDP stats from FlowMonitor for cross-check (check UDP port range)
        else if (t.destinationPort >= udpPortBase && t.destinationPort < udpPortBase + nUdpPairs)
        {
            double rxThroughput = iter->second.rxBytes * 8.0 / (simulationTime-2.0) / 1000000.0; // Mbps
            double avgDelay = iter->second.rxPackets>0 ? iter->second.delaySum.GetSeconds () / iter->second.rxPackets : 0;
            std::cout << "UDP Flow: " << t.sourceAddress << " --> " << t.destinationAddress
                      << " Port: " << t.destinationPort
                      << " RxBytes: " << iter->second.rxBytes
                      << " Throughput: " << rxThroughput << " Mbps"
                      << " AvgDelay: " << avgDelay << " s"
                      << " LostPackets: " << iter->second.lostPackets
                      << std::endl;
        }
    }
    std::cout << "=== Aggregate TCP Stats ===" << std::endl;
    std::cout << "Aggregate TCP throughput: " << tcpThroughput << " Mbps" << std::endl;
    std::cout << "Aggregate TCP avg delay: " << (nStarClients > 0 ? tcpDelay/nStarClients : 0.0) << " s" << std::endl;
    std::cout << "Aggregate TCP received-packets: " << tcpRxPackets << std::endl;
    std::cout << "Aggregate TCP lost-packets: " << tcpLostPackets << std::endl;

    // Simple UDP summary
    std::cout << "=== Simple UDP (first flow) Stats ===" << std::endl;
    std::cout << "UDP packets sent: " << udpPacketsSent << std::endl;
    std::cout << "UDP packets received: " << udpPacketsReceived << std::endl;
    double udpLoss = udpPacketsSent>0 ? 100.0 * (udpPacketsSent-udpPacketsReceived)/udpPacketsSent : 0;
    std::cout << "UDP Packet Loss (%): " << udpLoss << std::endl;
    std::cout << "UDP Throughput (first flow): ";
    if (udpLastReceivedTime > udpFirstSentTime && udpFirstSentTime>0)
    {
        // Assume packet size 1024 bytes
        double throughput = (udpPacketsReceived * 1024.0 * 8) / (udpLastReceivedTime - udpFirstSentTime) / 1e6;
        std::cout << throughput << " Mbps" << std::endl;
    }
    else
        std::cout << "(Insufficient data)" << std::endl;

    Simulator::Destroy ();
    return 0;
}