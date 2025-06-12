#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set simulation parameters
    uint32_t numClients = 5;
    Time simDuration = Seconds(10.0);

    // Enable logging
    LogComponentEnable("TcpSocketBase", LOG_LEVEL_INFO);

    // Create nodes: one server and multiple clients
    NodeContainer clients;
    clients.Create(numClients);
    NodeContainer server;
    server.Create(1);

    // Connect each client to the server using point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer clientDevices[numClients];
    NetDeviceContainer serverDevices[numClients];

    for (uint32_t i = 0; i < numClients; ++i) {
        NetDeviceContainer devices = p2p.Install(clients.Get(i), server.Get(0));
        clientDevices[i] = devices.Get(0);
        serverDevices[i] = devices.Get(1);
    }

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(clients);
    stack.Install(server);

    // Assign IP addresses
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer clientInterfaces[numClients];
    Ipv4InterfaceContainer serverInterfaces[numClients];

    for (uint32_t i = 0; i < numClients; ++i) {
        std::ostringstream subnet;
        subnet << "10.0." << i << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(clientDevices[i], serverDevices[i]));
        clientInterfaces[i] = interfaces.Get(0);
        serverInterfaces[i] = interfaces.Get(1);
    }

    // Set up TCP server application on the central server node
    uint16_t port = 5000;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(server.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(simDuration);

    // Set up TCP client applications on each client node
    for (uint32_t i = 0; i < numClients; ++i) {
        AddressValue remoteAddress(InetSocketAddress(serverInterfaces[i].GetAddress(0), port));
        BulkSendHelper bulkSend("ns3::TcpSocketFactory", Address());
        bulkSend.SetAttribute("Remote", remoteAddress);
        bulkSend.SetAttribute("MaxBytes", UintegerValue(0)); // Continuous sending

        ApplicationContainer clientApp = bulkSend.Install(clients.Get(i));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(simDuration);
    }

    // Setup FlowMonitor to collect throughput, latency, packet loss etc.
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowMonitor = flowmonHelper.InstallAll();

    // Setup NetAnim for visualization
    AnimationInterface anim("star-topology.xml");
    anim.SetConstantPosition(server.Get(0), 0, 0); // Server at center
    for (uint32_t i = 0; i < numClients; ++i) {
        double angle = 2 * M_PI * i / numClients;
        double x = 100 * cos(angle);
        double y = 100 * sin(angle);
        anim.SetConstantPosition(clients.Get(i), x, y);
    }

    // Run simulation
    Simulator::Stop(simDuration);
    Simulator::Run();

    // Print FlowMonitor results
    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow ID: " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000 << " Mbps\n";
        std::cout << "  Mean Delay: " << i->second.delaySum.GetSeconds() / i->second.rxPackets << " s\n";
    }

    Simulator::Destroy();
    return 0;
}