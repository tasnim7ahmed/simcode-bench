#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

#include <iostream>
#include <string>
#include <vector>

// Define constants
#define NUM_VEHICLES 5

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Vanet80211pSimulation");

int main(int argc, char *argv[])
{
    // 0. Configuration and Command Line Arguments
    LogComponentEnable("Vanet80211pSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    double simTime = 60.0;     // Total simulation time in seconds
    uint32_t packetSize = 1024; // Size of application packets in bytes
    double txInterval = 0.1;   // Time interval between packets for OnOffApplication in seconds
    std::string dataRate = "1Mbps"; // Data rate for OnOffApplication

    CommandLine cmd(__FILE__);
    cmd.AddValue("simTime", "Total simulation time (seconds)", simTime);
    cmd.AddValue("packetSize", "Size of application packets (bytes)", packetSize);
    cmd.AddValue("txInterval", "Time interval between packets (seconds)", txInterval);
    cmd.AddValue("dataRate", "Data rate for OnOffApplication", dataRate);
    cmd.Parse(argc, argv);

    // 1. Create Nodes (Vehicles)
    NodeContainer vehicles;
    vehicles.Create(NUM_VEHICLES);

    // 2. Mobility Model (ConstantVelocityMobilityModel for straight road with varying speeds)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicles);

    // Set initial positions and velocities for each vehicle
    // All moving in positive X direction
    // Vehicle 0 (Server): Starts at (0,0,0) and moves at 10 m/s
    // Vehicle 1: Starts at (50,0,0) and moves at 12 m/s
    // Vehicle 2: Starts at (100,0,0) and moves at 9 m/s
    // Vehicle 3: Starts at (150,0,0) and moves at 11 m/s
    // Vehicle 4: Starts at (200,0,0) and moves at 8 m/s
    std::vector<Vector> initialPositions = {
        Vector(0.0, 0.0, 0.0),
        Vector(50.0, 0.0, 0.0),
        Vector(100.0, 0.0, 0.0),
        Vector(150.0, 0.0, 0.0),
        Vector(200.0, 0.0, 0.0)
    };
    std::vector<Vector> velocities = {
        Vector(10.0, 0.0, 0.0),
        Vector(12.0, 0.0, 0.0),
        Vector(9.0, 0.0, 0.0),
        Vector(11.0, 0.0, 0.0),
        Vector(8.0, 0.0, 0.0)
    };

    for (uint32_t i = 0; i < NUM_VEHICLES; ++i)
    {
        Ptr<ConstantVelocityMobilityModel> mob = vehicles.Get(i)->GetObject<ConstantVelocityMobilityModel>();
        mob->SetPosition(initialPositions[i]);
        mob->SetVelocity(velocities[i]);
    }

    // 3. Wi-Fi (IEEE 802.11p) Setup
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211p);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("OfdmRate6Mbps"),
                                 "ControlMode", StringValue("OfdmRate6Mbps"));

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    // Using FriisPropagationLossModel for open-space propagation, 5.9 GHz for ITS
    wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel", "Frequency", DoubleValue(5.9e9));

    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetChannel(wifiChannel.Create());
    wifiPhy.SetTxPowerStart(0); // 0 dBm
    wifiPhy.SetTxPowerEnd(0);   // 0 dBm
    wifiPhy.SetRxSensitivity(-90); // -90 dBm (typical for 802.11p)

    NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
    wifiMac.SetType("ns3::AdhocWifiMac"); // Ad-hoc mode for V2V communication

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, vehicles);

    // 4. Install Internet Stack
    InternetStackHelper internet;
    internet.Install(vehicles);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // 5. Applications (UDP Client-Server Model)
    // Server: PacketSink on Node 0
    uint16_t port = 9;
    Address serverAddress = InetSocketAddress(interfaces.GetAddress(0), port);
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", serverAddress);
    ApplicationContainer serverApps = packetSinkHelper.Install(vehicles.Get(0));
    serverApps.Start(Seconds(1.0)); // Server starts early
    serverApps.Stop(Seconds(simTime));

    // Clients: OnOffApplication on Nodes 1 to 4, sending to Node 0 (the server)
    OnOffHelper onoff("ns3::UdpSocketFactory", Address()); // Address will be set dynamically
    // Configure OnOffApplication to be always ON to send data periodically
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
    // Set the interval between packets
    // This is achieved by setting the DataRate and PacketSize.
    // E.g., for 1Mbps (1000000 bps) and 1024 bytes (8192 bits) packet, interval = 8192/1000000 = 0.008192s
    // The `txInterval` variable is not directly used for OnOffApp with DataRate, but could be used to set the `OnTime` to control packet frequency if needed.
    // For this setup, the DataRate determines frequency.

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < NUM_VEHICLES; ++i)
    {
        AddressValue remoteAddress(InetSocketAddress(interfaces.GetAddress(0), port));
        onoff.SetAttribute("Remote", remoteAddress);
        clientApps.Add(onoff.Install(vehicles.Get(i)));
    }
    clientApps.Start(Seconds(2.0)); // Clients start after server
    clientApps.Stop(Seconds(simTime));

    // 6. NetAnim Visualization
    AnimationInterface anim("vanet-80211p.xml");
    anim.SetMaxPktsPerTraceFile(50000); // Increase if many packets are expected
    anim.EnablePacketMetadata(true); // Show packet info in NetAnim
    anim.EnableIpv4RouteTracking(
        "vanet-80211p-routing.xml",
        Seconds(0),
        Seconds(simTime),
        Seconds(1)
    ); // Track routing table changes (optional but good for debugging)

    // Optional: Set NetAnim node attributes (colors, descriptions)
    for (uint32_t i = 0; i < NUM_VEHICLES; ++i)
    {
        anim.UpdateNodeDescription(vehicles.Get(i), "Vehicle " + std::to_string(i));
        anim.UpdateNodeColor(vehicles.Get(i), 0, 255, 0); // Green for vehicles
        anim.UpdateNodeSize(vehicles.Get(i), 5.0, 5.0);
    }
    // Set a different color for the server node
    anim.UpdateNodeColor(vehicles.Get(0), 255, 0, 0); // Red for server

    // 7. FlowMonitor for Metrics Collection
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    // 8. Run Simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // 9. Collect and Display Metrics (Packet Delivery Ratio, End-to-End Delay)
    flowMonitor->CheckFor = FlowMonitor::LATENCY_STATS | FlowMonitor::TX_RX_STATS;
    flowMonitor->SerializeToXmlFile("vanet-flow-monitor.xml", true, true);

    double totalTxPackets = 0;
    double totalRxPackets = 0;
    double totalDelaySumSeconds = 0.0; // Accumulate total delay in seconds
    
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();

    NS_LOG_INFO("\n--- Flow Statistics ---");
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);

        // Filter flows: only consider client (source != server) to server (destination == server) flows
        if (t.destinationAddress == interfaces.GetAddress(0) && t.sourceAddress != interfaces.GetAddress(0))
        {
            NS_LOG_INFO("  Flow ID: " << i->first << " (Client " << t.sourceAddress << " -> Server " << t.destinationAddress << ")");
            NS_LOG_INFO("    Tx Packets: " << i->second.txPackets);
            NS_LOG_INFO("    Rx Packets: " << i->second.rxPackets);
            NS_LOG_INFO("    Lost Packets: " << i->second.lostPackets);
            NS_LOG_INFO("    Tx Bytes: " << i->second.txBytes);
            NS_LOG_INFO("    Rx Bytes: " << i->second.rxBytes);
            NS_LOG_INFO("    Throughput: " << i->second.rxBytes * 8.0 / (simTime * 1000 * 1000) << " Mbps");

            if (i->second.txPackets > 0)
            {
                totalTxPackets += i->second.txPackets;
                totalRxPackets += i->second.rxPackets;

                double pdr = (double)i->second.rxPackets / i->second.txPackets;
                NS_LOG_INFO("    Packet Delivery Ratio (PDR): " << pdr);

                if (i->second.rxPackets > 0)
                {
                    Time delay = i->second.delaySum / i->second.rxPackets;
                    NS_LOG_INFO("    Average End-to-End Delay: " << delay.GetSeconds() << " seconds");
                    totalDelaySumSeconds += delay.GetSeconds() * i->second.rxPackets; // Accumulate total delay
                }
            }
        }
    }

    NS_LOG_INFO("\n--- Overall Simulation Metrics ---");
    if (totalTxPackets > 0)
    {
        double overallPdr = totalRxPackets / totalTxPackets;
        NS_LOG_INFO("Total Client-Transmitted Packets: " << totalTxPackets);
        NS_LOG_INFO("Total Server-Received Packets: " << totalRxPackets);
        NS_LOG_INFO("Overall Packet Delivery Ratio: " << overallPdr);

        if (totalRxPackets > 0)
        {
            double averageOverallDelay = totalDelaySumSeconds / totalRxPackets;
            NS_LOG_INFO("Overall Average End-to-End Delay: " << averageOverallDelay << " seconds");
        }
        else
        {
            NS_LOG_INFO("No packets were received by the server to calculate average delay.");
        }
    }
    else
    {
        NS_LOG_WARN("No packets were transmitted by clients.");
    }

    Simulator::Destroy();

    return 0;
}