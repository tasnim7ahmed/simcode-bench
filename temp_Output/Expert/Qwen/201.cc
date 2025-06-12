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

    NodeContainer centralNode;
    centralNode.Create(1);

    NodeContainer peripheralNodes;
    peripheralNodes.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer centralDevices;
    NetDeviceContainer peripheralDevices;

    for (uint32_t i = 0; i < peripheralNodes.GetN(); ++i) {
        NetDeviceContainer link = p2p.Install(centralNode.Get(0), peripheralNodes.Get(i));
        centralDevices.Add(link.Get(0));
        peripheralDevices.Add(link.Get(1));
    }

    InternetStackHelper stack;
    stack.Install(centralNode);
    stack.Install(peripheralNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.255.0");

    Ipv4InterfaceContainer centralInterfaces;
    Ipv4InterfaceContainer peripheralInterfaces;

    for (uint32_t i = 0; i < peripheralNodes.GetN(); ++i) {
        Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(centralDevices.Get(i), peripheralDevices.Get(i)));
        centralInterfaces.Add(interfaces.Get(0));
        peripheralInterfaces.Add(interfaces.Get(1));
        address.NewNetwork();
    }

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(centralNode);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    for (uint32_t i = 0; i < peripheralNodes.GetN(); ++i) {
        UdpEchoClientHelper echoClient(centralInterfaces.GetAddress(0), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(20));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps = echoClient.Install(peripheralNodes.Get(i));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AsciiTraceHelper asciiTraceHelper;
    p2p.EnableAsciiAll(asciiTraceHelper.CreateFileStream("star-topology.tr"));
    p2p.EnablePcapAll("star-topology");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    std::ofstream logFile("metrics.csv");
    logFile << "FlowId,PacketDeliveryRatio,AvgDelay,Throughput\n";

    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        FlowIdTag flowId;
        iter->first->GetTag(flowId);
        uint32_t flowIdValue = flowId.GetFlowId();

        uint64_t txPackets = iter->second.txPackets;
        uint64_t rxPackets = iter->second.rxPackets;
        double packetDeliveryRatio = static_cast<double>(rxPackets) / txPackets;

        Time avgDelay = iter->second.delaySum / rxPackets;
        double throughput = (iter->second.rxBytes * 8.0) / (10.0 * 1000 * 1000); // Mbps

        logFile << flowIdValue << ","
                << packetDeliveryRatio << ","
                << avgDelay.GetSeconds() << ","
                << throughput << "\n";
    }

    logFile.close();
    Simulator::Destroy();
    return 0;
}