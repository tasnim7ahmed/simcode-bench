#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridTopologySimulation");

int main(int argc, char *argv[]) {
    uint32_t tcpClients = 5;
    uint32_t udpNodes = 6;
    double simDuration = 10.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("tcpClients", "Number of TCP clients in star topology", tcpClients);
    cmd.AddValue("udpNodes", "Number of UDP nodes in mesh topology", udpNodes);
    cmd.AddValue("simDuration", "Duration of simulation in seconds", simDuration);
    cmd.Parse(argc, argv);

    NodeContainer tcpStarNodes;
    tcpStarNodes.Create(tcpClients + 1); // +1 for server

    NodeContainer udpMeshNodes;
    udpMeshNodes.Create(udpNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer tcpDevices;
    InternetStackHelper stack;
    stack.InstallAll();

    // Star Topology: TCP Clients <-> Server
    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.255.0");
    for (uint32_t i = 0; i < tcpClients; ++i) {
        NetDeviceContainer dev = p2p.Install(NodeContainer(tcpStarNodes.Get(i), tcpStarNodes.Get(tcpClients)));
        tcpDevices.Add(dev);
        Ipv4InterfaceContainer intface = address.Assign(dev);
        address.NewNetwork();
    }

    // Mesh Topology: All UDP nodes connected to each other
    NetDeviceContainer udpDevices[6][6];
    for (uint32_t i = 0; i < udpNodes; ++i) {
        for (uint32_t j = i + 1; j < udpNodes; ++j) {
            NetDeviceContainer dev = p2p.Install(NodeContainer(udpMeshNodes.Get(i), udpMeshNodes.Get(j)));
            udpDevices[i][j] = dev;
            Ipv4InterfaceContainer intface = address.Assign(dev);
            address.NewNetwork();
        }
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // TCP Applications
    uint16_t port = 5000;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(tcpStarNodes.Get(tcpClients));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simDuration));

    OnOffHelper clientHelper("ns3::TcpSocketFactory", Address());
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

    for (uint32_t i = 0; i < tcpClients; ++i) {
        AddressValue remoteAddress(InetSocketAddress(tcpStarNodes.Get(tcpClients)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), port));
        clientHelper.SetAttribute("Remote", remoteAddress);
        ApplicationContainer clientApp = clientHelper.Install(tcpStarNodes.Get(i));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(simDuration - 1));
    }

    // UDP Applications
    port = 6000;
    UdpServerHelper udpServer(port);
    ApplicationContainer udpServerApps = udpServer.Install(udpMeshNodes);
    udpServerApps.Start(Seconds(0.0));
    udpServerApps.Stop(Seconds(simDuration));

    UdpClientHelper udpClient;
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));

    for (uint32_t i = 0; i < udpNodes; ++i) {
        for (uint32_t j = 0; j < udpNodes; ++j) {
            if (i != j) {
                Ptr<Node> srcNode = udpMeshNodes.Get(i);
                Ipv4Address dstAddr = srcNode->GetObject<Ipv4>()->GetAddress(srcNode->GetObject<Ipv4>()->GetInterfaceForDevice(udpMeshNodes.Get(j)->GetDevice(0)), 0).GetLocal();
                udpClient.SetRemote(dstAddr, port);
                ApplicationContainer clientApp = udpClient.Install(udpMeshNodes.Get(i));
                clientApp.Start(Seconds(1.0));
                clientApp.Stop(Seconds(simDuration - 1));
            }
        }
    }

    // Animation
    AnimationInterface anim("hybrid_topology.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    // Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();

    monitor->CheckForLostPackets();
    monitor->SerializeToXmlFile("hybrid_topology_flow.xml", true, true);

    Simulator::Destroy();
    return 0;
}