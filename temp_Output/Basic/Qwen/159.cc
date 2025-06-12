#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridTopologySimulation");

int main(int argc, char *argv[]) {
    // Enable logging for UdpEchoClient and UdpEchoServer
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer starNodes;
    starNodes.Create(4);  // Node 0 (central), Nodes 1-3 (peripheral)
    NodeContainer busNodes;
    busNodes.Create(3);   // Nodes 4,5,6

    // PointToPoint links
    PointToPointHelper p2pStar;
    p2pStar.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pStar.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2pBus;
    p2pBus.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pBus.SetChannelAttribute("Delay", StringValue("2ms"));

    // Star topology: connect peripheral nodes to central node (Node 0)
    NetDeviceContainer starDevices[3];
    for (uint32_t i = 0; i < 3; ++i) {
        starDevices[i] = p2pStar.Install(starNodes.Get(0), starNodes.Get(i + 1));
    }

    // Bus topology: connect Nodes 4-6 sequentially
    NetDeviceContainer busDevices[2];
    for (uint32_t i = 0; i < 2; ++i) {
        busDevices[i] = p2pBus.Install(busNodes.Get(i), busNodes.Get(i + 1));
    }

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper address;

    // Assign IPs for star topology
    Ipv4InterfaceContainer starInterfaces[3];
    for (uint32_t i = 0; i < 3; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        starInterfaces[i] = address.Assign(starDevices[i]);
    }

    // Assign IPs for bus topology
    Ipv4InterfaceContainer busInterfaces[2];
    for (uint32_t i = 0; i < 2; ++i) {
        std::ostringstream subnet;
        subnet << "10.2." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        busInterfaces[i] = address.Assign(busDevices[i]);
    }

    // Set up routing so that all nodes know about each other
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup UDP Echo Server on Node 0
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(starNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(20.0));

    // Setup UDP Echo Clients on Nodes 4 and 5
    Time interPacketInterval = Seconds(1.);
    uint32_t packetSize = 1024;
    uint32_t maxPacketCount = 20;
    UdpEchoClientHelper echoClient(starInterfaces[0].GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
    echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps;
    clientApps.Add(echoClient.Install(busNodes.Get(0))); // Node 4
    clientApps.Add(echoClient.Install(busNodes.Get(1))); // Node 5
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    // FlowMonitor setup
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Simulation run
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    // Output FlowMonitor results
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (auto [flowId, flowStats] : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowId);
        std::cout << "Flow ID: " << flowId << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << flowStats.txPackets << "\n";
        std::cout << "  Rx Packets: " << flowStats.rxPackets << "\n";
        std::cout << "  Lost Packets: " << flowStats.lostPackets << "\n";
        std::cout << "  Avg Delay: " << flowStats.delaySum.GetSeconds() / flowStats.rxPackets << " s\n";
    }

    Simulator::Destroy();
    return 0;
}