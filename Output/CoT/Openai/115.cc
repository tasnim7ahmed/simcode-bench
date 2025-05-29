#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocWifiRandomWalkSimulation");

int main(int argc, char *argv[])
{
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    uint32_t numNodes = 2;
    double simTime = 20.0; // seconds
    uint32_t packetSize = 1024;
    uint32_t numPackets = 20;
    double interval = 1.0; // seconds

    NodeContainer nodes;
    nodes.Create(numNodes);

    // Wi-Fi Setup
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility Model: RandomWalk2d within bounds
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)),
                              "Distance", DoubleValue(10.0),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue (20.0),
                                  "MinY", DoubleValue (20.0),
                                  "DeltaX", DoubleValue (30.0),
                                  "DeltaY", DoubleValue (30.0),
                                  "GridWidth", UintegerValue (2),
                                  "LayoutType", StringValue ("RowFirst"));
    mobility.Install(nodes);

    // Internet stack and IPv4 addresses
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP Echo Server on Node 0
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simTime));

    // UDP Echo Client on Node 1
    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApp = echoClient.Install(nodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(simTime));

    // Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Animation
    AnimationInterface anim("adhoc-wifi-randomwalk.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        anim.UpdateNodeDescription(nodes.Get(i), "Node" + std::to_string(i));
        anim.UpdateNodeColor(nodes.Get(i), (i == 0) ? 0 : 255, (i == 0) ? 255 : 0, 0); // Red for server, green for client
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Flow Monitor Statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalTxPackets = 0;
    double totalRxPackets = 0;
    double totalDelay = 0;
    double totalRxBytes = 0;
    double throughput = 0;

    std::cout << "Flow statistics:" << std::endl;
    for (const auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow ID: " << flow.first << " Src Addr: " << t.sourceAddress << " Dst Addr: " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets: " << flow.second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << flow.second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << flow.second.lostPackets << std::endl;
        std::cout << "  Tx Bytes: " << flow.second.txBytes << std::endl;
        std::cout << "  Rx Bytes: " << flow.second.rxBytes << std::endl;
        std::cout << "  Average Delay: " << (flow.second.rxPackets > 0 ? (flow.second.delaySum.GetSeconds() / flow.second.rxPackets) : 0) << " s" << std::endl;
        if (flow.second.timeLastRxPacket.GetSeconds() > 0 && flow.second.timeFirstTxPacket.GetSeconds() > 0)
        {
            throughput = flow.second.rxBytes * 8.0 / (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds()) / 1024;
            std::cout << "  Throughput: " << throughput << " Kbps" << std::endl;
        }
        else
        {
            std::cout << "  Throughput: 0 Kbps" << std::endl;
        }

        totalTxPackets += flow.second.txPackets;
        totalRxPackets += flow.second.rxPackets;
        totalDelay += flow.second.delaySum.GetSeconds();
        totalRxBytes += flow.second.rxBytes;
    }
    double packetDeliveryRatio = (totalTxPackets > 0) ? (totalRxPackets / totalTxPackets) * 100 : 0;
    double avgEndToEndDelay = (totalRxPackets > 0) ? totalDelay / totalRxPackets : 0;
    double totalThroughput = (simTime > 0) ? (totalRxBytes * 8.0 / (simTime*1024)) : 0;

    std::cout << "\n===== Simulation Performance Statistics =====" << std::endl;
    std::cout << "Total Sent Packets: " << totalTxPackets << std::endl;
    std::cout << "Total Received Packets: " << totalRxPackets << std::endl;
    std::cout << "Packet Delivery Ratio: " << packetDeliveryRatio << " %" << std::endl;
    std::cout << "Average End-to-End Delay: " << avgEndToEndDelay << " s" << std::endl;
    std::cout << "Throughput: " << totalThroughput << " Kbps" << std::endl;

    Simulator::Destroy();
    return 0;
}