#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/mesh-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridTopologySimulation");

void ThroughputPrinter(uint32_t nodeId, std::string proto, double throughput) {
    std::cout << "Node " << nodeId << " (" << proto << ") throughput: " << throughput << " Mbps" << std::endl;
}

int main(int argc, char *argv[]) {
    // Parameters
    uint32_t nStarClients = 4;
    uint32_t nMeshNodes = 5;
    double simTime = 20.0;
    uint16_t tcpPort = 8080;
    uint16_t udpPort = 9000;
    std::string dataRate = "10Mbps";
    std::string delay = "2ms";

    // Enable logging
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

    // STAR Topology (TCP)
    NodeContainer starServer;
    starServer.Create(1);

    NodeContainer starClients;
    starClients.Create(nStarClients);

    NodeContainer allStarNodes;
    allStarNodes.Add(starServer);
    allStarNodes.Add(starClients);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
    p2p.SetChannelAttribute("Delay", StringValue(delay));

    InternetStackHelper stack;
    stack.Install(allStarNodes);

    // Connect star clients to the server
    NetDeviceContainer starDevicesServer, starDevicesClients;
    std::vector<Ipv4InterfaceContainer> starInterfaces;
    Ipv4AddressHelper address;
    for (uint32_t i = 0; i < nStarClients; ++i) {
        NodeContainer pair;
        pair.Add(starClients.Get(i));
        pair.Add(starServer.Get(0));
        NetDeviceContainer devices = p2p.Install(pair);
        starDevicesClients.Add(devices.Get(0));
        starDevicesServer.Add(devices.Get(1));

        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer iface = address.Assign(devices);
        starInterfaces.push_back(iface);
    }

    // Install TCP server (PacketSink) on central server
    ApplicationContainer tcpSinkApps;
    for (uint32_t i = 0; i < nStarClients; ++i) {
        Address sinkAddress(InetSocketAddress(starInterfaces[i].GetAddress(1), tcpPort + i));
        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
        tcpSinkApps.Add(packetSinkHelper.Install(starServer.Get(0)));
    }
    tcpSinkApps.Start(Seconds(0.0));
    tcpSinkApps.Stop(Seconds(simTime));

    // Install TCP clients (OnOff) on star clients
    ApplicationContainer tcpClientApps;
    for (uint32_t i = 0; i < nStarClients; ++i) {
        OnOffHelper clientHelper("ns3::TcpSocketFactory",
                                 Address(InetSocketAddress(starInterfaces[i].GetAddress(1), tcpPort + i)));
        clientHelper.SetAttribute("DataRate", StringValue("2Mbps"));
        clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
        clientHelper.SetAttribute("StartTime", TimeValue(Seconds(1.0 + i)));
        clientHelper.SetAttribute("StopTime", TimeValue(Seconds(simTime - 1)));
        tcpClientApps.Add(clientHelper.Install(starClients.Get(i)));
    }

    // MESH Topology (UDP)
    NodeContainer meshNodes;
    meshNodes.Create(nMeshNodes);

    MeshHelper mesh;
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS);
    mesh.SetMacType("RandomStart", TimeValue(Seconds(0.01)));
    mesh.SetNumberOfInterfaces(1);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    NetDeviceContainer meshDevices = mesh.Install(wifiPhy, meshNodes);
    stack.Install(meshNodes);

    Ipv4AddressHelper meshAddress;
    meshAddress.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer meshInterfaces = meshAddress.Assign(meshDevices);

    // Install UDP echo server applications on all mesh nodes
    ApplicationContainer udpServerApps;
    for (uint32_t i = 0; i < nMeshNodes; ++i) {
        UdpEchoServerHelper echoServer(udpPort + i);
        udpServerApps.Add(echoServer.Install(meshNodes.Get(i)));
    }
    udpServerApps.Start(Seconds(0.5));
    udpServerApps.Stop(Seconds(simTime));

    // Install UDP clients (send to random other nodes)
    ApplicationContainer udpClientApps;
    for (uint32_t i = 0; i < nMeshNodes; ++i) {
        uint32_t dst = (i + 1) % nMeshNodes;
        UdpEchoClientHelper echoClient(meshInterfaces.GetAddress(dst), udpPort + dst);
        echoClient.SetAttribute("MaxPackets", UintegerValue(50));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.2)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));
        ApplicationContainer app = echoClient.Install(meshNodes.Get(i));
        app.Start(Seconds(1.0 + i));
        app.Stop(Seconds(simTime - 1));
        udpClientApps.Add(app);
    }

    // Enable pcap tracing
    p2p.EnablePcapAll("hybrid-tcp-star");
    wifiPhy.EnablePcapAll("hybrid-udp-mesh");

    // NetAnim setup, assign positions
    AnimationInterface anim("hybrid-topology.xml");
    for (uint32_t i = 0; i < nStarClients; ++i) {
        anim.SetConstantPosition(starClients.Get(i), 25 + 50 * i, 100);
    }
    anim.SetConstantPosition(starServer.Get(0), 125, 50);

    double meshRadius = 80.0;
    double meshCenterX = 350.0, meshCenterY = 150.0;
    for (uint32_t i = 0; i < nMeshNodes; ++i) {
        double angle = (2 * M_PI * i) / nMeshNodes;
        double x = meshCenterX + meshRadius * std::cos(angle);
        double y = meshCenterY + meshRadius * std::sin(angle);
        anim.SetConstantPosition(meshNodes.Get(i), x, y);
    }

    anim.UpdateNodeDescription(starServer.Get(0), "TCP Server");
    for (uint32_t i = 0; i < nStarClients; ++i) {
        anim.UpdateNodeDescription(starClients.Get(i), "TCP Client");
    }
    for (uint32_t i = 0; i < nMeshNodes; ++i) {
        anim.UpdateNodeDescription(meshNodes.Get(i), "Mesh Node");
    }

    // FlowMonitor for measurements
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Throughput, latency, packet loss measurements
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::cout << "\n==== Flow stats (All Protocols) ====\n";
    for (auto const &flow : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::string proto = (t.protocol == 6) ? "TCP" : ((t.protocol == 17) ? "UDP" : "OTHER");
        double throughput = (flow.second.rxBytes * 8.0) / (simTime * 1000000.0);
        double avgDelayMs = (flow.second.rxPackets > 0) ? (flow.second.delaySum.GetSeconds() / flow.second.rxPackets) * 1000.0 : 0.0;
        double packetLossRatio = (flow.second.txPackets > 0) ?
            ((double)(flow.second.txPackets - flow.second.rxPackets) / flow.second.txPackets) : 0.0;

        std::cout << "FlowId: " << flow.first
                  << ", Src: " << t.sourceAddress
                  << ", Dst: " << t.destinationAddress
                  << ", Protocol: " << proto
                  << ", Throughput: " << throughput << " Mbps"
                  << ", Mean Delay: " << avgDelayMs << " ms"
                  << ", Packet Loss: " << packetLossRatio * 100 << " %"
                  << std::endl;
    }

    Simulator::Destroy();
    return 0;
}