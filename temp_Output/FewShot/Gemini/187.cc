#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

// Constants for the Fat-Tree topology
const uint32_t K = 4; // Number of pods (and also switches per pod)
const uint32_t NUM_HOSTS = (K * K * K) / 4;

int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Enable logging for debugging
    LogComponentEnable("FatTreeSimulation", LOG_LEVEL_INFO);

    // Create Nodes
    NodeContainer coreNodes;
    NodeContainer aggregationNodes;
    NodeContainer edgeNodes;
    NodeContainer hostNodes;

    coreNodes.Create((K / 2) * (K / 2));
    aggregationNodes.Create(K * (K / 2));
    edgeNodes.Create(K * (K / 2));
    hostNodes.Create(NUM_HOSTS);

    // Install Internet Stack
    InternetStackHelper stack;
    stack.Install(coreNodes);
    stack.Install(aggregationNodes);
    stack.Install(edgeNodes);
    stack.Install(hostNodes);

    // Helper functions for creating links
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    // Create links between core and aggregation
    NetDeviceContainer coreAggDevices[(K / 2) * (K / 2)][K / 2];
    for (uint32_t i = 0; i < (K / 2); ++i) {
        for (uint32_t j = 0; j < (K / 2); ++j) {
            for (uint32_t p = 0; p < K; ++p) {
                NetDeviceContainer devices = pointToPoint.Install(coreNodes.Get(i * (K / 2) + j), aggregationNodes.Get(p * (K / 2) + i));
                coreAggDevices[i * (K / 2) + j][i] = devices;
            }
        }
    }

    // Create links between aggregation and edge
    NetDeviceContainer aggEdgeDevices[K * (K / 2)][K / 2];
    for (uint32_t p = 0; p < K; ++p) {
        for (uint32_t i = 0; i < (K / 2); ++i) {
            for (uint32_t j = 0; j < (K / 2); ++j) {
                NetDeviceContainer devices = pointToPoint.Install(aggregationNodes.Get(p * (K / 2) + i), edgeNodes.Get(p * (K / 2) + j));
                aggEdgeDevices[p * (K / 2) + i][j] = devices;
            }
        }
    }

    // Create links between edge and host
    NetDeviceContainer edgeHostDevices[K * (K / 2)][K / 2];
    for (uint32_t p = 0; p < K; ++p) {
        for (uint32_t i = 0; i < (K / 2); ++i) {
            for (uint32_t j = 0; j < (K / 2); ++j) {
                uint32_t hostIndexStart = (p * K * K) / 4;
                uint32_t hostIndex = hostIndexStart + (i * (K / 2)) + j;
                NetDeviceContainer devices = pointToPoint.Install(edgeNodes.Get(p * (K / 2) + i), hostNodes.Get(hostIndex));
                edgeHostDevices[p * (K / 2) + i][j] = devices;
            }
        }
    }

    // Assign IP Addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.0.0");
    Ipv4InterfaceContainer coreInterfaces;
    Ipv4InterfaceContainer aggregationInterfaces;
    Ipv4InterfaceContainer edgeInterfaces;
    Ipv4InterfaceContainer hostInterfaces;

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Assign IP addresses to hosts
    uint32_t nextHostAddress = 1; // Start at 10.0.1.1

    for (uint32_t i = 0; i < NUM_HOSTS; ++i) {
        address.AssignOne(edgeHostDevices[i / ((K *K)/4)][i % (K/2)]);
        nextHostAddress++;
    }


    // Create Applications (TCP client and server)
    uint16_t port = 50000; // Port for the server
    uint32_t maxBytes = 100000000; // Total bytes for the client to send

    // Install a BulkSendApplication on node 0
    BulkSendHelper source("ns3::TcpSocketFactory",
        InetSocketAddress(Ipv4Address("10.0.1.4"), port));  // server is hardcoded
    source.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    ApplicationContainer sourceApps = source.Install(hostNodes.Get(0));
    sourceApps.Start(Seconds(2.0)); // Start the client after 2 seconds
    sourceApps.Stop(Seconds(10.0)); // Stop the client after 10 seconds

    // Install a SinkApplication on node 3 (server)
    PacketSinkHelper sink("ns3::TcpSocketFactory",
        InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(hostNodes.Get(3));
    sinkApps.Start(Seconds(1.0)); // Start the server earlier
    sinkApps.Stop(Seconds(10.0)); // Stop the server at the end

    // Set up FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Animation Interface
    AnimationInterface anim("fat-tree.xml");

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Print FlowMonitor statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Delay Sum:    " << i->second.delaySum << "\n";
        std::cout << "  Jitter Sum:   " << i->second.jitterSum << "\n";
    }

    monitor->SerializeToXmlFile("fat-tree.flowmon", true, true);

    Simulator::Destroy();
    return 0;
}