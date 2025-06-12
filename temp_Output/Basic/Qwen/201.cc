#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyUdpEcho");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: 1 hub and 4 peripheral nodes
    NodeContainer hub;
    hub.Create(1);

    NodeContainer peripherals;
    peripherals.Create(4);

    // Connect each peripheral to the hub via point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devicesHub[4];
    NetDeviceContainer devicesPeriph[4];

    for (uint32_t i = 0; i < 4; ++i) {
        NodeContainer n(hub.Get(0), peripherals.Get(i));
        NetDeviceContainer devs = p2p.Install(n);
        devicesHub[i] = devs.Get(0);
        devicesPeriph[i] = devs.Get(1);
    }

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfacesHub[4];
    Ipv4InterfaceContainer interfacesPeriph[4];

    for (uint32_t i = 0; i < 4; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(devicesHub[i], devicesPeriph[i]));
        interfacesHub[i] = interfaces.Get(0);
        interfacesPeriph[i] = interfaces.Get(1);
    }

    // Install UDP echo server on the hub node
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(hub.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Install UDP echo clients on peripheral nodes
    for (uint32_t i = 0; i < 4; ++i) {
        UdpEchoClientHelper echoClient(interfacesHub[i].GetAddress(0), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(20));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps = echoClient.Install(peripherals.Get(i));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));
    }

    // Enable FlowMonitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowMonitor = flowmonHelper.InstallAll();

    // Enable pcap tracing
    p2p.EnablePcapAll("star_topology_udp_echo");

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Collect metrics
    flowMonitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();

    double totalTxPackets = 0, totalRxPackets = 0;
    double totalDelay = 0.0;
    double totalThroughput = 0.0;
    uint32_t flowCount = 0;

    for (auto it = stats.begin(); it != stats.end(); ++it) {
        FlowId flowId = it->first;
        FlowMonitor::FlowStats flowStats = it->second;

        totalTxPackets += flowStats.txPackets;
        totalRxPackets += flowStats.rxPackets;
        totalDelay += flowStats.delaySum.ToDouble(Time::S);
        totalThroughput += (flowStats.rxBytes * 8.0 / (flowStats.timeLastRxPacket.GetSeconds() - flowStats.timeFirstTxPacket.GetSeconds())) / 1e6;
        flowCount++;
    }

    double packetDeliveryRatio = (totalTxPackets > 0) ? (totalRxPackets / totalTxPackets) : 0.0;
    double averageDelay = (flowCount > 0) ? (totalDelay / flowCount) : 0.0;
    double averageThroughput = (flowCount > 0) ? (totalThroughput / flowCount) : 0.0;

    NS_LOG_UNCOND("Metrics Summary:");
    NS_LOG_UNCOND("Packet Delivery Ratio: " << packetDeliveryRatio);
    NS_LOG_UNCOND("Average End-to-End Delay: " << averageDelay << " s");
    NS_LOG_UNCOND("Average Throughput: " << averageThroughput << " Mbps");

    Simulator::Destroy();
    return 0;
}