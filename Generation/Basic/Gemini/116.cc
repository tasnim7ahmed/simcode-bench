#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

// NS_LOG_COMPONENT_DEFINE("AdHocTwoNodes");

int main(int argc, char *argv[]) {
    // Set default simulation parameters
    double simulationTime = 60.0; // seconds
    std::string phyMode = "OfdmRate6Mbps"; // DSSS, ErpOfdm or OfdmRate6Mbps

    // Command line arguments
    CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Total duration of the simulation in seconds", simulationTime);
    cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
    cmd.Parse(argc, argv);

    // Enable logging for specific components (optional)
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("WifiPhy", LOG_LEVEL_INFO);

    // 1. Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // 2. Configure mobility model (RandomWalk2dMobilityModel)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-500, 500, -500, 500)), // X and Y bounds
                              "Speed", StringValue("ns3::UniformRandomVariable[Min=1|Max=5]"), // 1-5 m/s
                              "Direction", StringValue("ns3::UniformRandomVariable[Min=0|Max=360]")); // 0-360 degrees
    mobility.Install(nodes);

    // 3. Configure IEEE 802.11 protocol in ad hoc mode
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a); // Or WIFI_STANDARD_80211n, etc.
    wifi.SetRemoteStationManager("ns3::AarfWifiManager"); // Automatic Rate Fallback

    YansWifiPhyHelper wifiPhy;
    // Set up the propagation loss model and channel
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO); // For pcap tracing
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationLossModel("ns3::LogDistancePropagationLossModel", "Exponent", DoubleValue(3.0));
    wifiChannel.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
    wifiPhy.SetChannel(wifiChannel.Create());

    // Set up the MAC layer for ad hoc mode
    NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
    wifiMac.SetType("ns3::AdhocWifiMac");

    // Install Wifi devices on nodes
    NetDeviceContainer wifiDevices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Enable pcap tracing on the wifi devices
    wifiPhy.EnablePcap("adhoc-two-nodes", wifiDevices);

    // 4. Install Internet stack on the nodes
    InternetStackHelper internet;
    internet.Install(nodes);

    // 5. Assign IPv4 addresses from the subnet 192.9.39.0/24
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer ipv4Interfaces = ipv4.Assign(wifiDevices);

    // 6. Implement UDP echo server and client applications (bidirectional)
    // Server on Node 0
    uint16_t port = 9; // Echo port
    UdpEchoServerHelper echoServer1(port);
    ApplicationContainer serverApps1 = echoServer1.Install(nodes.Get(0));
    serverApps1.Start(Seconds(1.0));
    serverApps1.Stop(Seconds(simulationTime - 1.0));

    // Server on Node 1
    UdpEchoServerHelper echoServer2(port);
    ApplicationContainer serverApps2 = echoServer2.Install(nodes.Get(1));
    serverApps2.Start(Seconds(1.0));
    serverApps2.Stop(Seconds(simulationTime - 1.0));

    // Client on Node 0 (sending to Node 1)
    UdpEchoClientHelper echoClient1(ipv4Interfaces.GetAddress(1), port);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(100)); // Limit packets to avoid infinite loop
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(0.5))); // 2 packets/sec
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024)); // 1024 bytes
    ApplicationContainer clientApps1 = echoClient1.Install(nodes.Get(0));
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(simulationTime - 0.5));

    // Client on Node 1 (sending to Node 0)
    UdpEchoClientHelper echoClient2(ipv4Interfaces.GetAddress(0), port);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps2 = echoClient2.Install(nodes.Get(1));
    clientApps2.Start(Seconds(2.0));
    clientApps2.Stop(Seconds(simulationTime - 0.5));

    // 7. Incorporate flow monitoring
    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.Install(nodes); // Install on all nodes

    // 8. Generate an animation file
    NetAnimator anim;
    anim.SetMobilityTraceFile("adhoc-mobility-trace.xml"); // Optional: output mobility trace
    anim.EnableAnimation("adhoc-animation.xml");

    // Configure simulation stop time
    Simulator::Stop(Seconds(simulationTime));

    // Run the simulation
    Simulator::Run();

    // 9. Output collected flow statistics and performance metrics
    monitor->CheckForLostPackets(); // Check for lost packets
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->Get = FlowStats();

    double totalTxPackets = 0;
    double totalRxPackets = 0;
    double totalTxBytes = 0;
    double totalRxBytes = 0;
    Time totalDelaySum = Seconds(0);
    int totalDeliveredFlows = 0;

    std::cout << "\n--- Flow Monitor Statistics ---\n";
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);

        std::cout << "Flow ID: " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Tx Bytes: " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes: " << i->second.rxBytes << "\n";

        totalTxPackets += i->second.txPackets;
        totalRxPackets += i->second.rxPackets;
        totalTxBytes += i->second.txBytes;
        totalRxBytes += i->second.rxBytes;

        if (i->second.rxPackets > 0) {
            double flowDuration = i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds();
            if (flowDuration > 0) {
                double throughput = (i->second.rxBytes * 8.0) / flowDuration / 1024 / 1024; // Mbps
                std::cout << "  Throughput: " << throughput << " Mbps\n";
            } else {
                std::cout << "  Throughput: 0 Mbps (Flow duration is zero)\n";
            }

            totalDelaySum += i->second.delaySum;
            totalDeliveredFlows++;
            std::cout << "  Mean Delay: " << (i->second.delaySum.GetSeconds() / i->second.rxPackets) * 1000 << " ms\n";
            std::cout << "  Jitter Sum: " << i->second.jitterSum.GetSeconds() * 1000 << " ms\n";
            std::cout << "  PDR: " << (double)i->second.rxPackets / i->second.txPackets * 100.0 << " %\n";
        } else {
            std::cout << "  No packets received in this flow.\n";
            std::cout << "  Throughput: 0 Mbps\n";
            std::cout << "  Mean Delay: N/A\n";
            std::cout << "  Jitter Sum: N/A\n";
            std::cout << "  PDR: 0 %\n";
        }
    }

    std::cout << "\n--- Overall Performance Metrics ---\n";
    if (totalTxPackets > 0) {
        double overallPdr = (double)totalRxPackets / totalTxPackets * 100.0;
        std::cout << "  Aggregate Packet Delivery Ratio (PDR): " << overallPdr << " %\n";
    } else {
        std::cout << "  Aggregate PDR: 0 % (No packets transmitted)\n";
    }

    if (totalRxPackets > 0) {
        // Calculate aggregate throughput based on active communication time (client start to simulation stop)
        // Adjust the denominator based on the actual time period where traffic was expected.
        // Here, apps start at 2.0s, sim ends at simulationTime, so active duration is (simulationTime - 2.0)
        double activeDuration = simulationTime - 2.0;
        if (activeDuration > 0) {
            double aggregateThroughput = (totalRxBytes * 8.0) / activeDuration / 1024 / 1024; // Mbps
            std::cout << "  Aggregate Throughput: " << aggregateThroughput << " Mbps\n";
        } else {
            std::cout << "  Aggregate Throughput: 0 Mbps (Active duration is zero)\n";
        }

        double averageEndToEndDelay = (totalDelaySum.GetSeconds() / totalRxPackets) * 1000; // ms
        std::cout << "  Average End-to-End Delay: " << averageEndToEndDelay << " ms\n";
    } else {
        std::cout << "  Aggregate Throughput: 0 Mbps (No packets received)\n";
        std::cout << "  Average End-to-End Delay: N/A\n";
    }

    // Clean up simulation resources
    Simulator::Destroy();

    return 0;
}