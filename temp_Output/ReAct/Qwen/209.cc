#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologySimulation");

int main(int argc, char *argv[]) {
    uint32_t numClients = 5;
    double simulationTime = 10.0;

    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));

    NodeContainer clients;
    clients.Create(numClients);

    NodeContainer server;
    server.Create(1);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer clientDevices[numClients];
    for (uint32_t i = 0; i < numClients; ++i) {
        clientDevices[i] = p2p.Install(clients.Get(i), server.Get(0));
    }

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[numClients + 1];

    for (uint32_t i = 0; i < numClients; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = address.Assign(clientDevices[i]);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 5000;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(server.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simulationTime));

    OnOffHelper onoff("ns3::TcpSocketFactory", Address());
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("MaxBytes", UintegerValue(0));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numClients; ++i) {
        onoff.SetAttribute("Remote", AddressValue(InetSocketAddress(interfaces[i].GetAddress(1), port)));
        clientApps.Add(onoff.Install(clients.Get(i)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime - 0.1));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AnimationInterface anim("star_topology.xml");
    for (uint32_t i = 0; i < numClients; ++i) {
        anim.UpdateNodeDescription(clients.Get(i), "Client-" + std::to_string(i));
    }
    anim.UpdateNodeDescription(server.Get(0), "Server");

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    double totalThroughput = 0;
    uint64_t totalLost = 0;
    double totalDelay = 0;
    uint32_t count = 0;

    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        FlowId flowId = iter->first;
        FlowMonitor::FlowStats st = iter->second;
        if (flowId > 1) {
            totalThroughput += (st.rxBytes * 8.0) / simulationTime / 1000 / 1000;
            totalLost += st.lostPackets;
            totalDelay += st.delaySum.GetSeconds() / st.rxPackets;
            count++;
        }
    }

    if (count > 0) {
        std::cout.precision(3);
        std::cout << "Average Throughput: " << (totalThroughput / count) << " Mbps" << std::endl;
        std::cout << "Total Packet Loss: " << totalLost << " packets" << std::endl;
        std::cout << "Average Latency: " << (totalDelay / count) << " s" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}