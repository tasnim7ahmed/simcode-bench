#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set simulation time
    double totalTime = 100.0;
    std::string animFile = "manet-aodv.xml";  // Output animation file

    // Create nodes (4 in total)
    NodeContainer nodes;
    nodes.Create(4);

    // Install AODV routing protocol
    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv);
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(nodes.Get(0), nodes.Get(1), nodes.Get(2), nodes.Get(3));

    // Setup mobility model: Random movement within 100x100 grid
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(10),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(nodes);

    // Configure UDP echo server on node 3
    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(3));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(totalTime));

    // Configure UDP echo client on node 0 to send packets to node 3
    UdpEchoClientHelper client(interfaces.GetAddress(3), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    client.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(totalTime));

    // Enable packet tracing
    AsciiTraceHelper asciiTraceHelper;
    pointToPoint.EnableAsciiAll(asciiTraceHelper.CreateFileStream("manet-aodv.tr"));

    // Setup NetAnim for visualization
    AnimationInterface anim(animFile);
    anim.EnablePacketMetadata(true);

    // Setup FlowMonitor to calculate throughput and packet loss
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowMonitor = flowmonHelper.InstallAll();

    // Run the simulation
    Simulator::Stop(Seconds(totalTime));
    Simulator::Run();

    // Print performance metrics
    flowMonitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = dynamic_cast<Ipv4FlowClassifier*>(flowmonHelper.GetClassifier())->FindFlow(iter->first);
        if (t.sourceAddress == interfaces.GetAddress(0) && t.destinationAddress == interfaces.GetAddress(3)) {
            std::cout << "\nFlow from " << t.sourceAddress << " (" << t.sourcePort << ") to "
                      << t.destinationAddress << " (" << t.destinationPort << ")"
                      << "\n  Tx Packets: " << iter->second.txPackets
                      << "\n  Rx Packets: " << iter->second.rxPackets
                      << "\n  Lost Packets: " << iter->second.lostPackets
                      << "\n  Throughput: " << (iter->second.rxBytes * 8.0) / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps" << std::endl;
        }
    }

    Simulator::Destroy();
    return 0;
}