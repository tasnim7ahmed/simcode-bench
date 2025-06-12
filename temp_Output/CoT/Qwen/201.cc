#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyUdpEcho");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer hub;
    hub.Create(1);

    NodeContainer spokes;
    spokes.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer hubDevices;
    NetDeviceContainer spokeDevices;

    for (uint32_t i = 0; i < spokes.GetN(); ++i) {
        NetDeviceContainer devices = p2p.Install(hub.Get(0), spokes.Get(i));
        hubDevices.Add(devices.Get(0));
        spokeDevices.Add(devices.Get(1));
    }

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.255.0");

    Ipv4InterfaceContainer hubInterfaces;
    Ipv4InterfaceContainer spokeInterfaces;

    for (uint32_t i = 0; i < spokes.GetN(); ++i) {
        Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(hubDevices.Get(i), spokeDevices.Get(i)));
        hubInterfaces.Add(interfaces.Get(0));
        spokeInterfaces.Add(interfaces.Get(1));
        address.NewNetwork();
    }

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(hub.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    for (uint32_t i = 0; i < spokes.GetN(); ++i) {
        UdpEchoClientHelper echoClient(hubInterfaces.GetAddress(0), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps = echoClient.Install(spokes.Get(i));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    p2p.EnablePcapAll("star_topology_udp_echo");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = static_cast<Ipv4FlowClassifier*>(flowmon.GetClassifier().Peek())->FindFlow(iter->first);
        std::cout << "Flow ID: " << iter->first << " Src Addr: " << t.sourceAddress << " Dst Addr: " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets: " << iter->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << iter->second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << iter->second.lostPackets << std::endl;
        std::cout << "  Average Delay: " << iter->second.delaySum.GetSeconds() / iter->second.rxPackets << " s" << std::endl;
        std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}