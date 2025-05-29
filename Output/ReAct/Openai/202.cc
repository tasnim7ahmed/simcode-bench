#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/mesh-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/sixlowpan-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WsnCircleSimulation");

static uint32_t packetsSent = 0;
static uint32_t packetsReceived = 0;
static double sumDelay = 0.0;

void RxSinkCallback(Ptr<const Packet> packet, const Address &address)
{
    packetsReceived++;
}

void RxPacketTrace(Ptr<const Packet> packet, const Address &from, const Address &to)
{
}

void TxPacketTrace(Ptr<const Packet> packet)
{
    packetsSent++;
}

void PacketDelayTrace(Ptr<const Packet> packet, Time delay)
{
    sumDelay += delay.GetSeconds();
}

int main(int argc, char *argv[])
{
    uint32_t numNodes = 6;
    double radius = 20.0;
    double simTime = 30.0;
    double packetInterval = 2.0;
    uint32_t packetSize = 60;
    bool enableTrace = true;
    std::string phyMode("DsssRate1Mbps");
    DataRate appDataRate("8kbps");
    
    CommandLine cmd(__FILE__);
    cmd.AddValue("simTime", "Simulation time in seconds.", simTime);
    cmd.Parse(argc, argv);

    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Set up IEEE 802.15.4 LrWpan net devices
    LrWpanHelper lrWpanHelper;
    NetDeviceContainer lrwpanDevices = lrWpanHelper.Install(nodes);

    // Assign short addresses
    for (uint32_t i = 0; i < lrwpanDevices.GetN(); ++i)
    {
        Ptr<LrWpanNetDevice> dev = DynamicCast<LrWpanNetDevice>(lrwpanDevices.Get(i));
        dev->GetMac()->SetShortAddress(Mac16Address::Allocate());
    }

    // Enable LrWpan logging (for debugging)
    // LogComponentEnable ("LrWpanMac", LOG_LEVEL_INFO);
    // LogComponentEnable ("LrWpanPhy", LOG_LEVEL_INFO);

    // 6LoWPAN enablement (to allow UDP/IP stack)
    SixLowPanHelper sixlowpanHelper;
    NetDeviceContainer sixlowpanDevices = sixlowpanHelper.Install(lrwpanDevices);

    // Mobility: circular topology
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        double angle = 2 * M_PI * i / numNodes;
        double x = radius * std::cos(angle);
        double y = radius * std::sin(angle);
        positionAlloc->Add(Vector(x, y, 0));
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Internet stack and assign IPv6 addresses
    InternetStackHelper internetv6;
    internetv6.Install(nodes);

    Ipv6AddressHelper address;
    address.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = address.Assign(sixlowpanDevices);
    // Remove link-local (first) address, keep global
    for (uint32_t i = 0; i < interfaces.GetN(); ++i)
    {
        interfaces.SetForwarding(i, true);
        interfaces.SetDefaultRouteInAllNodes(i);
    }

    // Sink: Set node 0 as data collector.
    uint16_t sinkPort = 5000;
    Address sinkAddress(Inet6SocketAddress(interfaces.GetAddress(0, 1), sinkPort));

    UdpServerHelper udpServer(sinkPort);
    ApplicationContainer serverApp = udpServer.Install(nodes.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simTime));

    // UDP Clients on all other nodes
    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < numNodes; ++i)
    {
        UdpClientHelper udpClient(interfaces.GetAddress(0, 1), sinkPort);
        udpClient.SetAttribute("MaxPackets", UintegerValue(0)); // Unlimited
        udpClient.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
        udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));
        clientApps.Add(udpClient.Install(nodes.Get(i)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simTime));

    // Connect stats callbacks
    Ptr<UdpServer> server = DynamicCast<UdpServer>(serverApp.Get(0));
    Config::ConnectWithoutContext("/NodeList/0/ApplicationList/0/$ns3::UdpServer/Rx", MakeCallback(&RxSinkCallback));

    // NetAnim setup
    if (enableTrace)
    {
        AnimationInterface anim("wsn-circle.xml");
        for (uint32_t i = 0; i < numNodes; ++i)
        {
            anim.UpdateNodeDescription(nodes.Get(i), i == 0 ? "Sink" : "Sensor");
            anim.UpdateNodeColor(nodes.Get(i), i == 0 ? 255 : 0, i == 0 ? 0 : 0, i == 0 ? 0 : 255);
        }
        anim.SetMobilityPollInterval(Seconds(1));
    }

    // FlowMonitor for stats
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Calculate/print performance metrics
    flowmon->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = flowmon->GetFlowStats();
    uint32_t udpTxPackets = 0, udpRxPackets = 0;
    uint64_t udpRxBytes = 0;
    double sumE2eDelay = 0;
    uint32_t flowCount = 0;
    for (auto it = stats.begin(); it != stats.end(); ++it)
    {
        udpTxPackets += it->second.txPackets;
        udpRxPackets += it->second.rxPackets;
        udpRxBytes += it->second.rxBytes;
        sumE2eDelay += it->second.delaySum.GetSeconds();
        flowCount++;
    }
    double pdr = udpTxPackets ? double(udpRxPackets) / udpTxPackets : 0;
    double avgE2eDelay = udpRxPackets ? sumE2eDelay / udpRxPackets : 0;
    double throughput = (simTime > 0) ? (udpRxBytes * 8.0) / simTime / 1000.0 : 0; // kbps

    std::cout << "----- Performance Metrics -----" << std::endl;
    std::cout << "Packets Sent:   " << udpTxPackets << std::endl;
    std::cout << "Packets Received: " << udpRxPackets << std::endl;
    std::cout << "Packet Delivery Ratio: " << pdr * 100 << "%" << std::endl;
    std::cout << "Average End-to-End Delay: " << avgE2eDelay * 1000 << " ms" << std::endl;
    std::cout << "Throughput: " << throughput << " kbps" << std::endl;

    Simulator::Destroy();
    return 0;
}