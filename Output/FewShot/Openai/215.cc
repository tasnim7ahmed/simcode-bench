#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging for AODV and UDP applications if needed
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Simulation parameters
    uint32_t numNodes = 4;
    double simTime = 30.0; // seconds

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Configure Wi-Fi (ad hoc)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility: RandomWayPoint within a 100x100 m area
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                             "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
                             "PositionAllocator", PointerValue(
                                 CreateObjectWithAttributes<RandomRectanglePositionAllocator>(
                                     "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                     "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]")
                                 )));
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(10.0),
                                 "MinY", DoubleValue(10.0),
                                 "DeltaX", DoubleValue(30.0),
                                 "DeltaY", DoubleValue(30.0),
                                 "GridWidth", UintegerValue(2),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.Install(nodes);

    // Install Internet stack with AODV routing
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // UDP Server on node 3
    uint16_t port = 9000;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    // UDP Client on node 0, send to node 3's IP
    UdpEchoClientHelper echoClient(interfaces.GetAddress(3), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.2))); // 5 packets per second
    echoClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime));

    // Enable packet capture
    wifiPhy.EnablePcapAll("manet-aodv");

    // Flow monitor
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Throughput & packet loss statistics
    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    uint64_t totalRxBytes = 0;
    uint64_t totalTxBytes = 0;
    uint32_t totalRxPackets = 0;
    uint32_t totalTxPackets = 0;
    for (const auto& flow : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        if ((t.sourceAddress == interfaces.GetAddress(0) && t.destinationAddress == interfaces.GetAddress(3)) ||
            (t.sourceAddress == interfaces.GetAddress(3) && t.destinationAddress == interfaces.GetAddress(0))) {
            totalRxBytes += flow.second.rxBytes;
            totalTxBytes += flow.second.txBytes;
            totalRxPackets += flow.second.rxPackets;
            totalTxPackets += flow.second.txPackets;
        }
    }
    double throughputKbps = totalRxBytes * 8.0 / simTime / 1024.0;
    std::cout << "Total transmitted packets: " << totalTxPackets << std::endl;
    std::cout << "Total received packets: " << totalRxPackets << std::endl;
    std::cout << "Packet loss: " << totalTxPackets - totalRxPackets << std::endl;
    std::cout << "Throughput: " << throughputKbps << " kbps" << std::endl;

    Simulator::Destroy();
    return 0;
}