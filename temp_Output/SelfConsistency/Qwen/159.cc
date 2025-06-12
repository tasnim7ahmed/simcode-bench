#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridStarBusTopology");

int main(int argc, char *argv[]) {
    // Enable logging for UdpEchoClient and Server
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes for star topology (Node 0 is central hub, Nodes 1-3 are peripherals)
    NodeContainer starHub;
    starHub.Create(1);  // Node 0

    NodeContainer starSpokes;
    starSpokes.Create(3);  // Nodes 1, 2, 3

    // Create nodes for bus topology (Nodes 4, 5, 6)
    NodeContainer busNodes;
    busNodes.Create(3);  // Nodes 4, 5, 6

    // Create point-to-point links for the star topology
    PointToPointHelper p2pStar;
    p2pStar.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pStar.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer starDevices[3];
    for (uint32_t i = 0; i < 3; ++i) {
        starDevices[i] = p2pStar.Install(starHub.Get(0), starSpokes.Get(i));
    }

    // Create point-to-point links for the bus topology
    PointToPointHelper p2pBus;
    p2pBus.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pBus.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer busDevices[2];
    busDevices[0] = p2pBus.Install(busNodes.Get(0), busNodes.Get(1)); // Node 4 <-> Node 5
    busDevices[1] = p2pBus.Install(busNodes.Get(1), busNodes.Get(2)); // Node 5 <-> Node 6

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    starHub.Add(starSpokes);
    stack.Install(starHub);
    stack.Install(busNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;

    Ipv4InterfaceContainer starInterfaces[3];
    for (uint32_t i = 0; i < 3; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        starInterfaces[i] = address.Assign(starDevices[i]);
    }

    Ipv4InterfaceContainer busInterfaces[2];
    address.SetBase("10.2.1.0", "255.255.255.0");
    busInterfaces[0] = address.Assign(busDevices[0]); // Node 4 & Node 5
    address.SetBase("10.2.2.0", "255.255.255.0");
    busInterfaces[1] = address.Assign(busDevices[1]); // Node 5 & Node 6

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up UDP echo server on Node 0 (central node of star)
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(starHub.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(20.0));

    // Set up UDP echo clients on Node 4 and Node 5 (from bus topology)
    Time interPacketInterval = Seconds(1.);
    uint32_t packetSize = 1024;
    uint32_t maxPacketCount = 20;

    UdpEchoClientHelper echoClient1(starInterfaces[0].GetAddress(0), 9); // Send to Node 0
    echoClient1.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
    echoClient1.SetAttribute("Interval", TimeValue(interPacketInterval));
    echoClient1.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp1 = echoClient1.Install(busNodes.Get(0)); // Node 4
    clientApp1.Start(Seconds(2.0));
    clientApp1.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient2(starInterfaces[0].GetAddress(0), 9); // Send to Node 0
    echoClient2.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
    echoClient2.SetAttribute("Interval", TimeValue(interPacketInterval));
    echoClient2.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp2 = echoClient2.Install(busNodes.Get(1)); // Node 5
    clientApp2.Start(Seconds(3.0));
    clientApp2.Stop(Seconds(20.0));

    // Install FlowMonitor to track performance metrics
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowMonitor = flowmonHelper.InstallAll();

    // Simulation setup
    Simulator::Stop(Seconds(20.0));

    // Run simulation
    Simulator::Run();

    // Output flow statistics
    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        std::cout << "Flow ID: " << iter->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << iter->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << iter->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << iter->second.lostPackets << "\n";
        std::cout << "  Average Delay: " << iter->second.delaySum.GetSeconds() / iter->second.rxPackets << "s\n";
        std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
    }

    Simulator::Destroy();
    return 0;
}