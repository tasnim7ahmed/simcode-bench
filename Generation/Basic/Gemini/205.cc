#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

#include <iostream>
#include <iomanip> // For std::fixed and std::setprecision
#include <limits>  // For std::numeric_limits

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VehicularNetwork");

int main (int argc, char *argv[])
{
    // 1. Configure Simulation Parameters
    double simTime = 60.0; // seconds
    uint32_t numVehicles = 4;
    uint32_t packetSize = 500; // bytes
    Time interPacketInterval = Seconds(1.0);
    uint32_t udpPort = 9;
    double vehicleSpeed = 10.0; // m/s
    double initialSpacing = 10.0; // meters between vehicles

    CommandLine cmd;
    cmd.AddValue ("simTime", "Total simulation time in seconds", simTime);
    cmd.AddValue ("numVehicles", "Number of vehicles", numVehicles);
    cmd.AddValue ("packetSize", "Size of UDP packets in bytes", packetSize);
    cmd.AddValue ("interPacketInterval", "Interval between UDP packets in seconds", interPacketInterval.GetSeconds());
    cmd.AddValue ("vehicleSpeed", "Speed of vehicles in m/s", vehicleSpeed);
    cmd.AddValue ("initialSpacing", "Initial spacing between vehicles in meters", initialSpacing);
    cmd.Parse (argc, argv);

    // Set default log level for relevant components
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("ConstantVelocityMobilityModel", LOG_LEVEL_INFO);
    LogComponentEnable("VehicularNetwork", LOG_LEVEL_INFO);

    // 2. Create Nodes
    NS_LOG_INFO ("Creating " << numVehicles << " vehicles.");
    NodeContainer vehicles;
    vehicles.Create(numVehicles);

    // 3. Install Mobility Model (Constant Velocity)
    NS_LOG_INFO ("Installing mobility model.");
    MobilityHelper mobility;
    mobility.SetMobilityModelType("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicles);

    // Set initial positions and velocities for each vehicle
    for (uint32_t i = 0; i < numVehicles; ++i)
    {
        Ptr<ConstantVelocityMobilityModel> mob = vehicles.Get(i)->GetObject<ConstantVelocityMobilityModel>();
        NS_ASSERT(mob != 0); // Ensure the mobility model is successfully installed
        mob->SetPosition(Vector(i * initialSpacing, 0, 0)); // Vehicles aligned along X-axis
        mob->SetVelocity(Vector(vehicleSpeed, 0, 0));       // All moving along positive X-axis
    }

    // 4. Install WiFi Devices (802.11p Ad-hoc mode)
    NS_LOG_INFO ("Installing WiFi devices.");
    Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default ();
    // Use ConstantRateWifiManager for simplicity, common in V2V simulations
    wifi80211p.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                        "DataMode", StringValue ("OfdmRate6Mbps"),
                                        "ControlMode", StringValue ("OfdmRate6Mbps"));

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    // Set up a channel suitable for vehicular environments
    Ptr<YansWifiChannel> wifiChannel = YansWifiChannelHelper::Default ()->Create ();
    wifiPhy.SetChannel (wifiChannel);

    // Use NistWifiMacHelper which provides 802.11p features
    NistWifiMacHelper wifiMac = NistWifiMacHelper::Default ();
    wifiMac.SetType ("ns3::AdhocWifiMac"); // Set MAC to Ad-hoc mode (IBSS)

    NetDeviceContainer devices = wifi80211p.Install (wifiPhy, wifiMac, vehicles);

    // Optional: Enable PCAP tracing for network devices
    // wifiPhy.EnablePcapAll ("vehicular-wifi", true);

    // 5. Install Internet Stack and Assign IP Addresses
    NS_LOG_INFO ("Installing Internet stack and assigning IP addresses.");
    InternetStackHelper stack;
    stack.Install(vehicles);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0"); // Assign addresses from 10.1.1.0/24
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // 6. Install Applications (UDP Servers and Clients for V2V communication)
    NS_LOG_INFO ("Installing UDP applications.");

    // Install UDP Servers on all nodes. Each node listens for incoming UDP packets.
    UdpServerHelper server(udpPort);
    ApplicationContainer serverApps = server.Install(vehicles);
    serverApps.Start(Seconds(1.0)); // Servers start early in the simulation
    serverApps.Stop(Seconds(simTime - 0.5)); // Servers stop slightly before simulation ends

    // Install UDP Clients. Each node sends periodic packets to all other nodes.
    for (uint32_t i = 0; i < numVehicles; ++i)
    {
        Ptr<Node> senderNode = vehicles.Get(i);
        for (uint32_t j = 0; j < numVehicles; ++j)
        {
            if (i == j) continue; // A node should not send packets to itself

            Ipv4Address receiverAddress = interfaces.GetAddress(j); // Get the IP address of the target node

            UdpClientHelper client (receiverAddress, udpPort);
            client.SetAttribute ("MaxPackets", UintegerValue (std::numeric_limits<uint32_t>::max())); // Send indefinitely
            client.SetAttribute ("Interval", TimeValue (interPacketInterval)); // Interval between packets
            client.SetAttribute ("PacketSize", UintegerValue (packetSize));     // Size of each packet
            
            ApplicationContainer clientApp = client.Install(senderNode);
            clientApp.Start(Seconds(2.0)); // Clients start after servers to allow setup
            clientApp.Stop(Seconds(simTime - 1.0)); // Clients stop before simulation ends
        }
    }

    // 7. Enable NetAnim Visualization
    NS_LOG_INFO ("Enabling NetAnim visualization.");
    NetAnimator anim;
    anim.SetMobilityData(vehicles); // Track node mobility
    anim.EnableIpv4RouteTracking(); // Visualize IP routes (if any are established)
    anim.EnableWifiChannels();      // Show WiFi links
    anim.EnableAnimation("vehicular-network.xml"); // Output to this file

    // 8. Setup Flow Monitor for Metrics Collection
    NS_LOG_INFO ("Setting up Flow Monitor.");
    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll(); // Install on all nodes

    // 9. Run Simulation
    NS_LOG_INFO ("Running simulation for " << simTime << " seconds.");
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // 10. Collect and Print Metrics
    NS_LOG_INFO ("Collecting and printing metrics.");
    monitor->CheckForExternalEvents (); // Aggregate statistics
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    uint64_t totalTxPackets = 0;
    uint64_t totalRxPackets = 0;
    uint64_t totalLostPackets = 0;
    double totalDelaySumNs = 0; // in nanoseconds
    double totalJitterSumNs = 0; // in nanoseconds

    std::cout << "\n--- Flow Monitor Statistics ---\n";
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Tx Bytes: " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes: " << i->second.rxBytes << "\n";

        if (i->second.rxPackets > 0)
        {
            std::cout << std::fixed << std::setprecision(2);
            std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
            std::cout << "  Average Delay: " << i->second.delaySum.GetSeconds() / i->second.rxPackets * 1000 << " ms\n";
            std::cout << "  Average Jitter: " << i->second.jitterSum.GetSeconds() / i->second.rxPackets * 1000 << " ms\n";
        }
        else
        {
            std::cout << "  No packets received for this flow.\n";
        }
        std::cout << "  Packet Loss Rate: " << (double)i->second.lostPackets / i->second.txPackets * 100.0 << "%\n";

        totalTxPackets += i->second.txPackets;
        totalRxPackets += i->second.rxPackets;
        totalLostPackets += i->second.lostPackets;
        totalDelaySumNs += i->second.delaySum.GetNanoSeconds();
        totalJitterSumNs += i->second.jitterSum.GetNanoSeconds();
    }

    std::cout << "\n--- Aggregate Statistics ---\n";
    std::cout << "Total Tx Packets: " << totalTxPackets << "\n";
    std::cout << "Total Rx Packets: " << totalRxPackets << "\n";
    std::cout << "Total Lost Packets: " << totalLostPackets << "\n";
    if (totalTxPackets > 0)
    {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Overall Packet Loss Rate: " << (double)totalLostPackets / totalTxPackets * 100.0 << "%\n";
    }
    else
    {
        std::cout << "No packets transmitted.\n";
    }

    if (totalRxPackets > 0)
    {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Overall Average Delay: " << totalDelaySumNs / totalRxPackets / 1000000.0 << " ms\n";
        std::cout << "Overall Average Jitter: " << totalJitterSumNs / totalRxPackets / 1000000.0 << " ms\n";
    }
    else
    {
        std::cout << "No packets received for overall delay/jitter calculation.\n";
    }
    std::cout << "----------------------------\n";

    // Optional: Serialize flow monitor data to XML file
    // monitor->SerializeToXmlFile("vehicular-flow.xml", true, true);

    // 11. Cleanup
    Simulator::Destroy();
    NS_LOG_INFO ("Done.");

    return 0;
}