#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MeshExample");

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponent::Enable("UdpClient", LOG_LEVEL_INFO);
    LogComponent::Enable("UdpServer", LOG_LEVEL_INFO);

    // Create mesh helper object
    MeshHelper meshHelper;
    meshHelper.SetStandard(WIFI_PHY_STANDARD_80211s);
    meshHelper.SetOperatingMode(WIFI_PHY_STANDARD_80211s, meshHelper.HT_SINGLE_MODE);

    // Create mesh nodes
    NodeContainer meshNodes;
    meshNodes.Create(4);

    // Install mesh on nodes
    meshHelper.Install(meshNodes);

    // Set up the internet stack
    InternetStackHelper internet;
    internet.Install(meshNodes);

    // Assign addresses to the interface
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(meshNodes);

    // Set static positions for the nodes
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(meshNodes);

    // Create UDP server on node 0
    UdpServerHelper server(9);
    ApplicationContainer serverApps = server.Install(meshNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(15.0));

    // Create UDP client on node 3
    UdpClientHelper client(interfaces.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = client.Install(meshNodes.Get(3));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(15.0));

    // Enable PCAP tracing
    PointToPointHelper pointToPoint;
    pointToPoint.EnablePcapAll("mesh-example");

    // FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run the simulation
    Simulator::Stop(Seconds(15.0));

    // NetAnim
    AnimationInterface anim("mesh-example.xml");
    anim.SetMaxPktsPerTraceFile(500000);

    Simulator::Run();

    // Print flow statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
    }

    Simulator::Destroy();

    return 0;
}