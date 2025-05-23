#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/tcp-westwood-plus.h"
#include "ns3/flow-monitor-module.h"

NS_LOG_COMPONENT_DEFINE("MultiInterfaceStaticRouting");

// Global pointers and variables for dynamic routing table manipulation on the source node
ns3::Ptr<ns3::Ipv4StaticRouting> g_staticRoutingSrc;
ns3::Ipv4Address g_dstIp;
ns3::Ipv4Address g_rtr1_ip_for_src;
ns3::Ipv4Address g_rtr2_ip_for_src;
uint32_t g_src_if_idx_to_rtr1;
uint32_t g_src_if_idx_to_rtr2;

// Function to set up route for path 1 (via Rtr1) on the source node
void SetupRoutePath1() {
    NS_LOG_INFO("Time " << ns3::Simulator::Now().GetSeconds() << "s: Setting route for " << g_dstIp << " via Rtr1 (" << g_rtr1_ip_for_src << ")");
    int32_t existingRoute = g_staticRoutingSrc->GetRouteForDest(g_dstIp);
    if (existingRoute != -1) {
        g_staticRoutingSrc->RemoveRoute(static_cast<uint32_t>(existingRoute));
    }
    // AddHostRouteTo(destination, gateway, interfaceIndex)
    g_staticRoutingSrc->AddHostRouteTo(g_dstIp, g_rtr1_ip_for_src, g_src_if_idx_to_rtr1);
}

// Function to set up route for path 2 (via Rtr2) on the source node
void SetupRoutePath2() {
    NS_LOG_INFO("Time " << ns3::Simulator::Now().GetSeconds() << "s: Setting route for " << g_dstIp << " via Rtr2 (" << g_rtr2_ip_for_src << ")");
    int32_t existingRoute = g_staticRoutingSrc->GetRouteForDest(g_dstIp);
    if (existingRoute != -1) {
        g_staticRoutingSrc->RemoveRoute(static_cast<uint32_t>(existingRoute));
    }
    // AddHostRouteTo(destination, gateway, interfaceIndex)
    g_staticRoutingSrc->AddHostRouteTo(g_dstIp, g_rtr2_ip_for_src, g_src_if_idx_to_rtr2);
}

int main(int argc, char *argv[]) {
    // Configure TCP
    ns3::Config::SetDefault("ns3::TcpL4Protocol::SocketType", ns3::StringValue("ns3::TcpNewReno"));
    ns3::Config::SetDefault("ns3::TcpSocket::SndBufSize", ns3::UintegerValue(1 << 20)); // 1MB
    ns3::Config::SetDefault("ns3::TcpSocket::RcvBufSize", ns3::UintegerValue(1 << 20)); // 1MB

    // Simulation parameters
    double simulationTime = 20.0; // Total duration of the simulation in seconds
    uint32_t dataRateMbps = 50;   // Data rate of P2P links in Mbps
    uint32_t linkDelayMs = 10;    // Delay of P2P links in milliseconds
    uint32_t pktSize = 1024;      // TCP packet size in bytes
    uint32_t appDuration = 5;     // Duration of each application transmission in seconds

    // Command line arguments for customization
    ns3::CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Total duration of the simulation in seconds", simulationTime);
    cmd.AddValue("dataRateMbps", "Data rate of P2P links in Mbps", dataRateMbps);
    cmd.AddValue("linkDelayMs", "Delay of P2P links in milliseconds", linkDelayMs);
    cmd.AddValue("pktSize", "TCP packet size in bytes", pktSize);
    cmd.AddValue("appDuration", "Duration of each application transmission in seconds", appDuration);
    cmd.Parse(argc, argv);

    // Enable logging
    ns3::LogComponentEnable("MultiInterfaceStaticRouting", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("TcpSocketBase", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("BulkSendApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("PacketSink", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("Ipv4L3Protocol", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("Ipv4StaticRouting", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("DropTailQueue", ns3::LOG_LEVEL_DEBUG);

    // Create nodes
    ns3::NodeContainer srcNode; srcNode.Create(1);      // Node 0: Src
    ns3::NodeContainer rtr1Node; rtr1Node.Create(1);    // Node 1: Rtr1
    ns3::NodeContainer rtr2Node; rtr2Node.Create(1);    // Node 2: Rtr2
    ns3::NodeContainer rtrDstNode; rtrDstNode.Create(1); // Node 3: RtrDst
    ns3::NodeContainer dstNode; dstNode.Create(1);      // Node 4: Dst

    // Install Internet Stack on all nodes
    ns3::InternetStackHelper stack;
    stack.Install(srcNode);
    stack.Install(rtr1Node);
    stack.Install(rtr2Node);
    stack.Install(rtrDstNode);
    stack.Install(dstNode);

    // Point-to-Point Helper configuration
    ns3::PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", ns3::StringValue(std::to_string(dataRateMbps) + "Mbps"));
    p2p.SetChannelAttribute("Delay", ns3::StringValue(std::to_string(linkDelayMs) + "ms"));
    // Set a large queue to minimize drops unless explicitly desired for congestion
    p2p.SetQueue("ns3::DropTailQueue", "MaxBytes", ns3::StringValue("1000000")); // 1MB queue

    // Create links and assign IP addresses
    ns3::NetDeviceContainer srcRtr1Devices, srcRtr2Devices, rtr1RtrDstDevices, rtr2RtrDstDevices, rtrDstDstDevices;
    ns3::Ipv4AddressHelper ipv4;

    // Link 1: Src (0) <-> Rtr1 (1) (10.1.1.0/24)
    srcRtr1Devices = p2p.Install(srcNode.Get(0), rtr1Node.Get(0));
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer srcRtr1Interfaces = ipv4.Assign(srcRtr1Devices);
    ns3::Ipv4Address srcIp_srcRtr1 = srcRtr1Interfaces.GetAddress(0); // 10.1.1.1
    ns3::Ipv4Address rtr1Ip_srcRtr1 = srcRtr1Interfaces.GetAddress(1); // 10.1.1.2

    // Link 2: Src (0) <-> Rtr2 (2) (10.1.2.0/24)
    srcRtr2Devices = p2p.Install(srcNode.Get(0), rtr2Node.Get(0));
    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer srcRtr2Interfaces = ipv4.Assign(srcRtr2Devices);
    ns3::Ipv4Address srcIp_srcRtr2 = srcRtr2Interfaces.GetAddress(0); // 10.1.2.1
    ns3::Ipv4Address rtr2Ip_srcRtr2 = srcRtr2Interfaces.GetAddress(1); // 10.1.2.2

    // Link 3: Rtr1 (1) <-> RtrDst (3) (10.1.3.0/24)
    rtr1RtrDstDevices = p2p.Install(rtr1Node.Get(0), rtrDstNode.Get(0));
    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer rtr1RtrDstInterfaces = ipv4.Assign(rtr1RtrDstDevices);
    ns3::Ipv4Address rtr1Ip_rtr1RtrDst = rtr1RtrDstInterfaces.GetAddress(0); // 10.1.3.1
    ns3::Ipv4Address rtrDstIp_rtr1RtrDst = rtr1RtrDstInterfaces.GetAddress(1); // 10.1.3.2

    // Link 4: Rtr2 (2) <-> RtrDst (3) (10.1.4.0/24)
    rtr2RtrDstDevices = p2p.Install(rtr2Node.Get(0), rtrDstNode.Get(0));
    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer rtr2RtrDstInterfaces = ipv4.Assign(rtr2RtrDstDevices);
    ns3::Ipv4Address rtr2Ip_rtr2RtrDst = rtr2RtrDstInterfaces.GetAddress(0); // 10.1.4.1
    ns3::Ipv4Address rtrDstIp_rtr2RtrDst = rtr2RtrDstInterfaces.GetAddress(1); // 10.1.4.2

    // Link 5: RtrDst (3) <-> Dst (4) (10.1.5.0/24)
    rtrDstDstDevices = p2p.Install(rtrDstNode.Get(0), dstNode.Get(0));
    ipv4.SetBase("10.1.5.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer rtrDstDstInterfaces = ipv4.Assign(rtrDstDstDevices);
    ns3::Ipv4Address rtrDstIp_rtrDstDst = rtrDstDstInterfaces.GetAddress(0); // 10.1.5.1
    g_dstIp = rtrDstDstInterfaces.GetAddress(1); // 10.1.5.2 (Destination node IP)

    // Set global variables for source node's routing functions
    g_rtr1_ip_for_src = rtr1Ip_srcRtr1; // Rtr1's IP on the Src-Rtr1 link (10.1.1.2)
    g_rtr2_ip_for_src = rtr2Ip_srcRtr2; // Rtr2's IP on the Src-Rtr2 link (10.1.2.2)

    // Get interface indices for Src node (for static routing on Src)
    ns3::Ptr<ns3::Ipv4> srcIpv4 = srcNode.Get(0)->GetObject<ns3::Ipv4>();
    g_src_if_idx_to_rtr1 = srcIpv4->GetInterfaceIndex(srcRtr1Devices.Get(0));
    g_src_if_idx_to_rtr2 = srcIpv4->GetInterfaceIndex(srcRtr2Devices.Get(0));

    // Get Static Routing Protocols for routers and source
    ns3::Ptr<ns3::Ipv4StaticRouting> rtr1StaticRouting = ns3::Ipv4StaticRoutingHelper().Get(rtr1Node.Get(0)->GetObject<ns3::Ipv4>());
    ns3::Ptr<ns3::Ipv4StaticRouting> rtr2StaticRouting = ns3::Ipv4StaticRoutingHelper().Get(rtr2Node.Get(0)->GetObject<ns3::Ipv4>());
    ns3::Ptr<ns3::Ipv4StaticRouting> rtrDstStaticRouting = ns3::Ipv4StaticRoutingHelper().Get(rtrDstNode.Get(0)->GetObject<ns3::Ipv4>());
    g_staticRoutingSrc = ns3::Ipv4StaticRoutingHelper().Get(srcNode.Get(0)->GetObject<ns3::Ipv4>());

    // Get Ipv4 pointers and interface indices for routers for route configuration
    ns3::Ptr<ns3::Ipv4> rtr1Ipv4 = rtr1Node.Get(0)->GetObject<ns3::Ipv4>();
    uint32_t rtr1_if_idx_to_src = rtr1Ipv4->GetInterfaceIndex(srcRtr1Devices.Get(1)); // Rtr1's side of Src-Rtr1 link
    uint32_t rtr1_if_idx_to_rtrdst = rtr1Ipv4->GetInterfaceIndex(rtr1RtrDstDevices.Get(0)); // Rtr1's side of Rtr1-RtrDst link

    ns3::Ptr<ns3::Ipv4> rtr2Ipv4 = rtr2Node.Get(0)->GetObject<ns3::Ipv4>();
    uint32_t rtr2_if_idx_to_src = rtr2Ipv4->GetInterfaceIndex(srcRtr2Devices.Get(1)); // Rtr2's side of Src-Rtr2 link
    uint32_t rtr2_if_idx_to_rtrdst = rtr2Ipv4->GetInterfaceIndex(rtr2RtrDstDevices.Get(0)); // Rtr2's side of Rtr2-RtrDst link

    ns3::Ptr<ns3::Ipv4> rtrDstIpv4 = rtrDstNode.Get(0)->GetObject<ns3::Ipv4>();
    uint32_t rtrDst_if_idx_to_rtr1 = rtrDstIpv4->GetInterfaceIndex(rtr1RtrDstDevices.Get(1)); // RtrDst's side of Rtr1-RtrDst link
    uint32_t rtrDst_if_idx_to_rtr2 = rtrDstIpv4->GetInterfaceIndex(rtr2RtrDstDevices.Get(1)); // RtrDst's side of Rtr2-RtrDst link

    // Add static routes to Rtr1
    // To Dst node (10.1.5.2) via RtrDst (10.1.3.2)
    rtr1StaticRouting->AddHostRouteTo(g_dstIp, rtrDstIp_rtr1RtrDst, rtr1_if_idx_to_rtrdst);
    // Return route for Rtr1: to reach Src's 10.1.2.0/24 subnet (from Rtr2 side of Src) via Src's 10.1.1.1 interface
    rtr1StaticRouting->AddNetworkRouteTo(srcIp_srcRtr2.GetNetwork(), srcIp_srcRtr2.GetMask(), srcIp_srcRtr1, rtr1_if_idx_to_src);

    // Add static routes to Rtr2
    // To Dst node (10.1.5.2) via RtrDst (10.1.4.2)
    rtr2StaticRouting->AddHostRouteTo(g_dstIp, rtrDstIp_rtr2RtrDst, rtr2_if_idx_to_rtrdst);
    // Return route for Rtr2: to reach Src's 10.1.1.0/24 subnet (from Rtr1 side of Src) via Src's 10.1.2.1 interface
    rtr2StaticRouting->AddNetworkRouteTo(srcIp_srcRtr1.GetNetwork(), srcIp_srcRtr1.GetMask(), srcIp_srcRtr2, rtr2_if_idx_to_src);

    // Add static routes to RtrDst
    // To reach 10.1.1.0/24 (Src-Rtr1 subnet) via Rtr1 (10.1.3.1)
    rtrDstStaticRouting->AddNetworkRouteTo(srcIp_srcRtr1.GetNetwork(), srcIp_srcRtr1.GetMask(), rtr1Ip_rtr1RtrDst, rtrDst_if_idx_to_rtr1);
    // To reach 10.1.2.0/24 (Src-Rtr2 subnet) via Rtr2 (10.1.4.1)
    rtrDstStaticRouting->AddNetworkRouteTo(srcIp_srcRtr2.GetNetwork(), srcIp_srcRtr2.GetMask(), rtr2Ip_rtr2RtrDst, rtrDst_if_idx_to_rtr2);

    // Applications setup
    ns3::uint16_t port = 9; // Common port for TCP applications

    // Server: PacketSink on Dst Node
    ns3::PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), port));
    ns3::ApplicationContainer serverApps = sinkHelper.Install(dstNode);
    serverApps.Start(ns3::Seconds(0.0));
    serverApps.Stop(ns3::Seconds(simulationTime + 1.0)); // Run slightly longer than simulation

    // Client 1 (Path 1 via Rtr1): BulkSendApplication on Src Node
    ns3::BulkSendHelper clientHelper1("ns3::TcpSocketFactory", ns3::InetSocketAddress(g_dstIp, port));
    clientHelper1.SetAttribute("MaxBytes", ns3::UintegerValue(pktSize * 10000)); // Send significant data
    clientHelper1.SetAttribute("SendSize", ns3::UintegerValue(pktSize));
    ns3::ApplicationContainer clientApps1 = clientHelper1.Install(srcNode);

    // Client 2 (Path 2 via Rtr2): BulkSendApplication on Src Node
    ns3::BulkSendHelper clientHelper2("ns3::TcpSocketFactory", ns3::InetSocketAddress(g_dstIp, port));
    clientHelper2.SetAttribute("MaxBytes", ns3::UintegerValue(pktSize * 10000));
    clientHelper2.SetAttribute("SendSize", ns3::UintegerValue(pktSize));
    ns3::ApplicationContainer clientApps2 = clientHelper2.Install(srcNode);

    // Schedule static routing changes and application starts
    ns3::Time path1StartTime = ns3::Seconds(1.0);
    ns3::Time path2StartTime = ns3::Seconds(path1StartTime.GetSeconds() + appDuration + 1.0); // Start after first app finishes + 1s buffer

    ns3::Simulator::Schedule(path1StartTime, &SetupRoutePath1);
    clientApps1.Start(path1StartTime + ns3::MilliSeconds(10)); // Start app slightly after route is set
    clientApps1.Stop(path1StartTime + ns3::Seconds(appDuration));

    ns3::Simulator::Schedule(path2StartTime, &SetupRoutePath2);
    clientApps2.Start(path2StartTime + ns3::MilliSeconds(10));
    clientApps2.Stop(path2StartTime + ns3::Seconds(appDuration));

    // Flow Monitor setup for verifying paths taken
    ns3::FlowMonitorHelper flowmon;
    ns3::Ptr<ns3::FlowMonitor> monitor = flowmon.InstallAll();

    ns3::Simulator::Stop(ns3::Seconds(simulationTime));
    ns3::Simulator::Run();

    // Export flow monitor results to XML
    monitor->SerializeToXmlFile("multi-interface-static-routing.flowmon", true, true);

    ns3::Simulator::Destroy();

    return 0;
}