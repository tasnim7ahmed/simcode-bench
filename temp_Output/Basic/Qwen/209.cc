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
    uint32_t nClients = 5;
    Time simulationTime = Seconds(10);
    std::string protocol = "Tcp";

    CommandLine cmd(__FILE__);
    cmd.AddValue("nClients", "Number of client nodes", nClients);
    cmd.AddValue("simulationTime", "Total simulation time", simulationTime);
    cmd.Parse(argc, argv);

    NodeContainer clients;
    clients.Create(nClients);

    NodeContainer server;
    server.Create(1);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer clientDevices[nClients];
    NetDeviceContainer serverDevices[nClients];

    for (uint32_t i = 0; i < nClients; ++i) {
        NetDeviceContainer devices = p2p.Install(clients.Get(i), server.Get(0));
        clientDevices[i] = devices.Get(0);
        serverDevices[i] = devices.Get(1);
    }

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[nClients + 1];

    for (uint32_t i = 0; i < nClients; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = address.Assign(clientDevices[i]);
        interfaces[nClients] = address.Assign(serverDevices[i]);
    }

    uint16_t port = 5000;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper(protocol, sinkLocalAddress);
    ApplicationContainer sinkApps = packetSinkHelper.Install(server.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(simulationTime);

    OnOffHelper onoff(protocol, Address());
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;

    for (uint32_t i = 0; i < nClients; ++i) {
        AddressValue remoteAddress(InetSocketAddress(interfaces[nClients].GetAddress(0), port));
        onoff.SetAttribute("Remote", remoteAddress);
        ApplicationContainer app = onoff.Install(clients.Get(i));
        app.Start(Seconds(1.0));
        app.Stop(simulationTime - Seconds(1));
        clientApps.Add(app);
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AnimationInterface anim("star_topology.xml");
    anim.SetConstantPosition(server.Get(0), 0, 0);
    for (uint32_t i = 0; i < nClients; ++i) {
        double angle = 2 * M_PI * i / nClients;
        double x = 10 * cos(angle);
        double y = 10 * sin(angle);
        anim.SetConstantPosition(clients.Get(i), x, y);
    }

    Simulator::Stop(simulationTime);
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = static_cast<Ipv4FlowClassifier*>(monitor->GetClassifier())->FindFlow(iter->first);
        std::cout << "Flow ID: " << iter->first << " Src Addr: " << t.sourceAddress << " Dst Addr: " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets: " << iter->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << iter->second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << iter->second.lostPackets << std::endl;
        std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000 << " Mbps" << std::endl;
        std::cout << "  Mean Delay: " << iter->second.delaySum.GetSeconds() / iter->second.rxPackets << " s" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}