#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/rng-seed-manager.h"

int main(int argc, char *argv[])
{
    // 1. Simulation Parameters
    uint32_t numNodes = 10;
    double simTime = 30.0; // seconds
    double areaX = 500.0;  // meters
    double areaY = 500.0;  // meters
    uint32_t numFlows = 5; // Number of UDP flows to create

    // 2. Command line arguments
    ns3::CommandLine cmd(__FILE__);
    cmd.AddValue("numNodes", "Number of nodes", numNodes);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.Parse(argc, argv);

    // Set a fixed seed for reproducibility
    ns3::RngSeedManager::SetSeed(112233);
    ns3::RngSeedManager::SetRun(1);

    // 3. Create Nodes
    ns3::NodeContainer nodes;
    nodes.Create(numNodes);

    // 4. Configure Mobility
    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", ns3::StringValue("0|500|0|500"),
                              "Speed", ns3::StringValue("ns3::ConstantRandomVariable[Constant=5.0]"), // 5 m/s
                              "Pause", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1.0]")); // 1 sec pause
    mobility.Install(nodes);

    // 5. Configure Wifi
    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_STANDARD_80211a); // Using 802.11a for better performance/range in ad-hoc

    ns3::YansWifiPhyHelper phy;
    phy.SetPcapDataLinkType(ns3::WifiPhyHelper::DLT_IEEE802_11_RADIO);
    ns3::YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel"); // Friis model for open space
    ns3::Ptr<ns3::WifiChannel> wifiChannel = channel.Create();
    phy.SetChannel(wifiChannel);

    ns3::WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    ns3::NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // 6. Install Internet Stack with AODV
    ns3::AodvHelper aodv;
    ns3::InternetStackHelper stack;
    stack.SetRoutingHelper(aodv);
    stack.Install(nodes);

    // 7. Assign IP Addresses
    ns3::Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // 8. Pcap Tracing
    phy.EnablePcap("aodv-ad-hoc", devices);

    // 9. UDP Traffic Generation
    ns3::UniformRandomVariable randNodeId;
    randNodeId.SetAttribute("Min", ns3::DoubleValue(0.0));
    randNodeId.SetAttribute("Max", ns3::DoubleValue(numNodes - 1.0));

    ns3::UniformRandomVariable randStartTime;
    randStartTime.SetAttribute("Min", ns3::DoubleValue(1.0)); // Start after 1 sec
    randStartTime.SetAttribute("Max", ns3::DoubleValue(simTime - 2.0)); // End before simTime - 2 sec

    uint16_t port = 9; // Starting port for UDP applications

    for (uint32_t i = 0; i < numFlows; ++i)
    {
        uint32_t srcNodeId = static_cast<uint32_t>(randNodeId.GetValue());
        uint32_t destNodeId = static_cast<uint32_t>(randNodeId.GetValue());

        // Ensure source and destination nodes are different
        while (srcNodeId == destNodeId)
        {
            destNodeId = static_cast<uint32_t>(randNodeId.GetValue());
        }

        ns3::Ptr<ns3::Node> srcNode = nodes.Get(srcNodeId);
        ns3::Ptr<ns3::Node> destNode = nodes.Get(destNodeId);
        ns3::Ipv4Address destAddress = interfaces.GetAddress(destNodeId);

        // UDP Server
        ns3::UdpServerHelper server(port);
        ns3::ApplicationContainer serverApps = server.Install(destNode);
        serverApps.Start(ns3::Seconds(0.0));
        serverApps.Stop(ns3::Seconds(simTime));

        // UDP Client
        ns3::UdpClientHelper client(destAddress, port);
        client.SetAttribute("MaxPackets", ns3::UintegerValue(1000000)); // Send many packets
        client.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(0.1))); // 10 packets per second
        client.SetAttribute("PacketSize", ns3::UintegerValue(1024)); // 1024 bytes per packet

        ns3::ApplicationContainer clientApps = client.Install(srcNode);
        double startTime = randStartTime.GetValue();
        clientApps.Start(ns3::Seconds(startTime));
        clientApps.Stop(ns3::Seconds(simTime)); // Client runs until simTime
        port++; // Use a different port for each flow
    }

    // 10. Flow Monitor
    ns3::FlowMonitorHelper flowMonitor;
    ns3::Ptr<ns3::FlowMonitor> monitor = flowMonitor.Install(nodes);

    // 11. Run Simulation
    ns3::Simulator::Stop(ns3::Seconds(simTime));
    ns3::Simulator::Run();

    // 12. Collect and Display Statistics
    monitor->CheckFor,; // Ensure final stats are collected for ns-3.41

    ns3::Ptr<ns3::Ipv4FlowClassifier> classifier = ns3::DynamicCast<ns3::Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    std::map<ns3::FlowId, ns3::FlowMonitor::FlowStats> stats = monitor->Get =;

    double totalTxPackets = 0;
    double totalRxPackets = 0;
    double totalDelaySum = 0;

    for (std::map<ns3::FlowId, ns3::FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        ns3::Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);

        // Only consider UDP flows that are transmitting
        if (t.protocol == 17 /* UDP */)
        {
            totalTxPackets += i->second.txPackets;
            totalRxPackets += i->second.rxPackets;

            if (i->second.rxPackets > 0)
            {
                totalDelaySum += i->second.delaySum.GetSeconds();
            }
        }
    }

    // Calculate PDR
    double pdr = 0;
    if (totalTxPackets > 0)
    {
        pdr = (totalRxPackets * 100.0) / totalTxPackets;
    }

    // Calculate average end-to-end delay
    double avgDelay = 0;
    if (totalRxPackets > 0)
    {
        avgDelay = totalDelaySum / totalRxPackets;
    }

    std::cout << "Simulation Results:" << std::endl;
    std::cout << "  Total packets sent: " << totalTxPackets << std::endl;
    std::cout << "  Total packets received: " << totalRxPackets << std::endl;
    std::cout << "  Packet Delivery Ratio (PDR): " << pdr << "%" << std::endl;
    std::cout << "  Average End-to-End Delay: " << avgDelay << " seconds" << std::endl;

    // 13. Cleanup
    ns3::Simulator::Destroy();

    return 0;
}