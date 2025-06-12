#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DctcpTwoBottleneckTopology");

int main(int argc, char *argv[]) {
    uint16_t numFlows = 40;
    uint16_t numSendersT1 = 30;
    uint16_t numSendersT2 = 20;
    uint32_t payloadSize = 1448;
    Time simulationTime = Seconds(5.0);
    Time startupWindow = Seconds(1.0);
    Time convergenceTime = Seconds(3.0);
    Time measurementInterval = Seconds(1.0);

    std::string transportProtocol = "TcpSocketFactory";
    bool enableFqCodel = false;
    bool enableEcn = true;

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpSocketBase::GetTypeId()));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(payloadSize));
    Config::SetDefault("ns3::TcpSocketBase::UseEcn", StringValue(enableEcn ? "On" : "Off"));
    Config::SetDefault("ns3::TcpSocketBase::EcnMode", StringValue("ClassicEcn"));

    NodeContainer hostsT1, hostsT2, switches, routers;
    hostsT1.Create(numSendersT1);
    hostsT2.Create(numSendersT2);
    switches.Create(2);
    routers.Create(1);

    PointToPointHelper hostLink, switchLink, routerLink;
    hostLink.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Gbps")));
    hostLink.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));
    switchLink.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Gbps")));
    switchLink.SetChannelAttribute("Delay", TimeValue(MicroSeconds(500)));
    routerLink.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    routerLink.SetChannelAttribute("Delay", TimeValue(MicroSeconds(500)));

    TrafficControlHelper tchRed;
    tchRed.SetRootQueueDisc("ns3::RedQueueDisc",
                            "MaxSize", StringValue("1000p"),
                            "MinTh", DoubleValue(5),
                            "MaxTh", DoubleValue(15),
                            "ECN", BooleanValue(true));

    NetDeviceContainer hostDevicesT1, hostDevicesT2, switchDevices, routerDevices;
    for (uint16_t i = 0; i < numSendersT1; ++i) {
        hostDevicesT1.Add(hostLink.Install(hostsT1.Get(i), switches.Get(0)));
    }
    for (uint16_t i = 0; i < numSendersT2; ++i) {
        hostDevicesT2.Add(hostLink.Install(hostsT2.Get(i), switches.Get(0)));
    }

    switchDevices = switchLink.Install(switches.Get(0), switches.Get(1));
    routerDevices = routerLink.Install(switches.Get(1), routers.Get(0));

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    for (uint16_t i = 0; i < hostDevicesT1.GetN(); ++i) {
        address.Assign(hostDevicesT1.Get(i));
    }
    for (uint16_t i = 0; i < hostDevicesT2.GetN(); ++i) {
        address.Assign(hostDevicesT2.Get(i));
    }

    Ipv4InterfaceContainer switchInterfaces = address.Assign(switchDevices);
    Ipv4InterfaceContainer routerInterfaces = address.Assign(routerDevices);

    tchRed.Install(switchDevices);
    tchRed.Install(routerDevices);

    uint16_t sinkPort = 8080;
    PacketSinkHelper packetSink(transportProtocol, InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps;
    for (uint32_t i = 0; i < routers.GetN(); ++i) {
        sinkApps.Add(packetSink.Install(routers.Get(i)));
    }
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(simulationTime);

    OnOffHelper clientSource(transportProtocol, InetSocketAddress(routerInterfaces.GetAddress(0), sinkPort));
    clientSource.SetAttribute("PacketSize", UintegerValue(payloadSize));
    clientSource.SetAttribute("MaxBytes", UintegerValue(0));

    ApplicationContainer clientApps;
    Time interFlowStart = startupWindow / numFlows;
    for (uint32_t i = 0; i < numFlows; ++i) {
        clientApps.Add(clientSource.Install((i < numSendersT1) ? hostsT1.Get(i) : hostsT2.Get(i - numSendersT1)));
        clientApps.Get(i)->SetStartTime(Seconds(0.0) + i * interFlowStart);
        clientApps.Get(i)->SetStopTime(simulationTime);
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(simulationTime);
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    double totalThroughput = 0;
    double fairnessSum = 0;
    uint32_t flowCount = 0;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        if (i->first > 0 && i->second.timeLastRxPacket >= convergenceTime &&
            i->second.timeFirstTxPacket <= convergenceTime + measurementInterval) {
            double duration = (std::min(i->second.timeLastRxPacket, convergenceTime + measurementInterval) - convergenceTime).GetSeconds();
            double bytes = i->second.rxBytes;
            double throughput = bytes * 8.0 / duration / 1e6;
            totalThroughput += throughput;
            fairnessSum += throughput * throughput;
            flowCount++;
            std::cout << "Flow " << i->first << ": " << throughput << " Mbps" << std::endl;
        }
    }

    double avgThroughput = totalThroughput / flowCount;
    double fairness = (flowCount > 0) ? (avgThroughput * avgThroughput) / (fairnessSum / flowCount) : 0;
    std::cout << "Aggregate Throughput: " << totalThroughput << " Mbps" << std::endl;
    std::cout << "Fairness Index: " << fairness << std::endl;

    Simulator::Destroy();
    return 0;
}