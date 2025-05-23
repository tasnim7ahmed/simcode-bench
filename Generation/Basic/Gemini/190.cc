#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/aodv-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

// For standard namespaces
using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetAodvSimulation");

int main(int argc, char* argv[])
{
    // 1. Set up simulation parameters
    double simTime = 100.0;     // seconds
    uint32_t numNodes = 3;
    uint32_t packetSize = 1024; // bytes
    double packetInterval = 0.5; // seconds (send a packet every 0.5 seconds)
    uint16_t udpPort = 9;       // Discard port

    // 2. Configure logging
    LogComponentEnable("ManetAodvSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("AodvRoutingProtocol", LOG_LEVEL_ALL); // Log AODV protocol events
    LogComponentEnable("WifiPhy", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    // 3. Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // 4. Set up mobility for random direction movement
    MobilityHelper mobility;
    // Set position allocator for initial placement
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("-50|50"),
                                  "Y", StringValue("-50|50"));
    // Set mobility model to RandomDirection2dMobilityModel
    mobility.SetMobilityModel("ns3::RandomDirection2dMobilityModel",
                              "Speed", PointerValue(CreateObject<UniformRandomVariable>("Min", 5.0, "Max", 15.0)),
                              "Pause", PointerValue(CreateObject<ConstantRandomVariable>("Constant", 0.5)), // 0.5s pause
                              "Bounds", RectangleValue(Rectangle(-100, 100, -100, 100))); // Larger movement area
    mobility.Install(nodes);

    // 5. Install Internet Stack with AODV routing protocol
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv); // Set AODV as the routing protocol
    internet.Install(nodes);

    // 6. Set up WiFi devices for Ad-hoc (IBSS) network
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac"); // Ad-hoc network (IBSS)

    WifiPhyHelper wifiPhy;
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO); // For Wireshark compatibility
    // Set transmit power (optional, can affect range and connectivity)
    // wifiPhy.Set("TxPowerStart", DoubleValue(15.0)); // dBm
    // wifiPhy.Set("TxPowerEnd", DoubleValue(15.0));   // dBm

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a); // Using 802.11a (5GHz, higher bandwidth)
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("OfdmRate6Mbps"), // Data rate
                                 "ControlMode", StringValue("OfdmRate6Mbps")); // Control rate

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // 7. Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // 8. Set up UDP applications
    // Flow 1: Node 0 (client) sends to Node 1 (server)
    ApplicationContainer clientApps1;
    ApplicationContainer serverApps1;

    // Server on Node 1
    PacketSinkHelper sinkHelper1("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), udpPort));
    serverApps1.Add(sinkHelper1.Install(nodes.Get(1)));
    serverApps1.Start(Seconds(0.0));
    serverApps1.Stop(Seconds(simTime));

    // Client on Node 0
    UdpClientHelper clientHelper1(interfaces.GetAddress(1), udpPort);
    clientHelper1.SetAttribute("MaxPackets", UintegerValue(4294967295U)); // Send "infinite" packets
    clientHelper1.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
    clientHelper1.SetAttribute("PacketSize", UintegerValue(packetSize));
    clientApps1.Add(clientHelper1.Install(nodes.Get(0)));
    clientApps1.Start(Seconds(1.0)); // Start client after 1 sec
    clientApps1.Stop(Seconds(simTime - 1.0)); // Stop 1 sec before sim end

    // Flow 2: Node 1 (client) sends to Node 2 (server)
    ApplicationContainer clientApps2;
    ApplicationContainer serverApps2;

    // Server on Node 2
    PacketSinkHelper sinkHelper2("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), udpPort));
    serverApps2.Add(sinkHelper2.Install(nodes.Get(2)));
    serverApps2.Start(Seconds(0.0));
    serverApps2.Stop(Seconds(simTime));

    // Client on Node 1
    UdpClientHelper clientHelper2(interfaces.GetAddress(2), udpPort);
    clientHelper2.SetAttribute("MaxPackets", UintegerValue(4294967295U));
    clientHelper2.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
    clientHelper2.SetAttribute("PacketSize", UintegerValue(packetSize));
    clientApps2.Add(clientHelper2.Install(nodes.Get(1)));
    clientApps2.Start(Seconds(2.0)); // Start client after 2 sec
    clientApps2.Stop(Seconds(simTime - 1.0));

    // 9. Enable NetAnim for visualization
    NetAnimator anim;
    anim.SetOutputFileName("manet-aodv.xml");
    anim.EnablePacketMetadata(true);       // Show packet details
    anim.EnableIpv4RouteTracking(true);    // Track IP routes for AODV visualization
    anim.EnableWifiMacTracer();            // Track WiFi MAC events for connectivity
    anim.TraceAll(); // Trace all nodes, devices, and applications

    // 10. Install FlowMonitor for detailed statistics
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // 11. Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // 12. Collect and print metrics from FlowMonitor
    monitor->CheckFor -->; // Ensure all latest statistics are collected
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalTxPackets = 0;
    double totalRxPackets = 0;
    double totalDelaySum = 0;
    uint32_t totalPacketsReceived = 0;

    std::cout << "\n--- Flow Statistics ---\n";
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow ID: " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes: " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes: " << i->second.rxBytes << "\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";

        if (i->second.txPackets > 0)
        {
            totalTxPackets += i->second.txPackets;
        }

        if (i->second.rxPackets > 0)
        {
            double packetDeliveryRatio = (double)i->second.rxPackets / i->second.txPackets;
            double avgDelay = (i->second.delaySum.GetSeconds() / i->second.rxPackets);
            // double avgJitter = (i->second.jitterSum.GetSeconds() / (i->second.rxPackets - 1)); // Jitter requires at least 2 received packets

            std::cout << "  Packet Delivery Ratio: " << packetDeliveryRatio * 100 << "%\n";
            std::cout << "  Average Delay: " << avgDelay * 1000 << " ms\n";
            // if (i->second.rxPackets > 1) {
            //     std::cout << "  Average Jitter: " << avgJitter * 1000 << " ms\n";
            // }

            totalRxPackets += i->second.rxPackets;
            totalDelaySum += i->second.delaySum.GetSeconds();
            totalPacketsReceived += i->second.rxPackets;
        }
        else
        {
            std::cout << "  No packets received for this flow.\n";
        }
        std::cout << "--------------------------------\n";
    }

    std::cout << "\n--- Aggregate Statistics ---\n";
    if (totalTxPackets > 0)
    {
        std::cout << "Total Tx Packets: " << totalTxPackets << "\n";
        std::cout << "Total Rx Packets: " << totalRxPackets << "\n";
        std::cout << "Overall Packet Delivery Ratio: " << (totalRxPackets / totalTxPackets) * 100 << "%\n";
        if (totalPacketsReceived > 0)
        {
            std::cout << "Overall Average Latency: " << (totalDelaySum / totalPacketsReceived) * 1000 << " ms\n";
        }
        else
        {
            std::cout << "No packets received in the simulation.\n";
        }
    }
    else
    {
        std::cout << "No packets sent in the simulation.\n";
    }

    // 13. Cleanup
    Simulator::Destroy();

    return 0;
}