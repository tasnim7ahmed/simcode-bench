#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpUdpComparisonSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("TcpEchoClientApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t tcpPort = 9;
    UdpEchoServerHelper udpServer(tcpPort);
    ApplicationContainer udpServerApps = udpServer.Install(nodes.Get(1));
    udpServerApps.Start(Seconds(1.0));
    udpServerApps.Stop(Seconds(10.0));

    UdpEchoClientHelper udpClient(interfaces.GetAddress(1), tcpPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(100000));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.001)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer udpClientApps = udpClient.Install(nodes.Get(0));
    udpClientApps.Start(Seconds(1.0));
    udpClientApps.Stop(Seconds(10.0));

    uint16_t udpPort = 20;
    TcpEchoServerHelper tcpServer(udpPort);
    ApplicationContainer tcpServerApps = tcpServer.Install(nodes.Get(1));
    tcpServerApps.Start(Seconds(1.0));
    tcpServerApps.Stop(Seconds(10.0));

    TcpEchoClientHelper tcpClient(interfaces.GetAddress(1), udpPort);
    tcpClient.SetAttribute("MaxPackets", UintegerValue(100000));
    tcpClient.SetAttribute("Interval", TimeValue(Seconds(0.001)));
    tcpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer tcpClientApps = tcpClient.Install(nodes.Get(0));
    tcpClientApps.Start(Seconds(1.0));
    tcpClientApps.Stop(Seconds(10.0));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AnimationInterface anim("tcp_udp_comparison.xml");
    anim.SetConstantPosition(nodes.Get(0), 0, 0);
    anim.SetConstantPosition(nodes.Get(1), 10, 0);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto it = stats.begin(); it != stats.end(); ++it) {
        FlowIdTag flowId;
        if (it->second.txPackets > 0 && it->second.rxPackets > 0) {
            double throughput = (it->second.rxBytes * 8.0) / (it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds()) / 1000000;
            Time delay = it->second.delaySum / it->second.rxPackets;
            double packetDeliveryRatio = static_cast<double>(it->second.rxPackets) / static_cast<double>(it->second.txPackets);
            NS_LOG_INFO("Flow ID: " << it->first << " Protocol: " << (it->second.protocol == 6 ? "TCP" : "UDP")
                        << " Throughput: " << throughput << " Mbps"
                        << " Avg Delay: " << delay.As(Time::MS)
                        << " Packet Delivery Ratio: " << packetDeliveryRatio);
        }
    }

    Simulator::Destroy();
    return 0;
}