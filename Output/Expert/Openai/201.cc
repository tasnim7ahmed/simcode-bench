#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    uint32_t numPeripheral = 4;
    NodeContainer hubNode;
    hubNode.Create(1);

    NodeContainer peripheralNodes;
    peripheralNodes.Create(numPeripheral);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));
    p2p.EnablePcapAll("star", false);

    InternetStackHelper stack;
    stack.Install(hubNode);
    stack.Install(peripheralNodes);

    Ipv4AddressHelper address;
    std::vector<NetDeviceContainer> deviceContainers(numPeripheral);
    std::vector<Ipv4InterfaceContainer> interfaceContainers(numPeripheral);

    for (uint32_t i = 0; i < numPeripheral; ++i)
    {
        NodeContainer pair(hubNode.Get(0), peripheralNodes.Get(i));
        deviceContainers[i] = p2p.Install(pair);

        std::ostringstream subnet;
        subnet << "10.1." << (i+1) << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaceContainers[i] = address.Assign(deviceContainers[i]);
    }

    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(hubNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    for (uint32_t i = 0; i < numPeripheral; ++i)
    {
        UdpEchoClientHelper echoClient(interfaceContainers[i].GetAddress(0), echoPort);
        echoClient.SetAttribute("MaxPackets", UintegerValue(10));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = echoClient.Install(peripheralNodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Metrics
    double totalTxPackets = 0;
    double totalRxPackets = 0;
    double totalDelay = 0;
    double totalThroughput = 0;

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
    for (const auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        if (t.destinationPort == echoPort)
        {
            totalTxPackets += flow.second.txPackets;
            totalRxPackets += flow.second.rxPackets;
            if (flow.second.rxPackets > 0)
            {
                totalDelay += (flow.second.delaySum.GetSeconds() / flow.second.rxPackets);
            }
            double throughput = (flow.second.rxBytes * 8.0) / (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds()) / 1024;
            totalThroughput += throughput;
        }
    }
    double pdr = (totalTxPackets > 0) ? (totalRxPackets / totalTxPackets) : 0;
    double avgDelay = (totalRxPackets > 0) ? (totalDelay / numPeripheral) : 0;
    double avgThroughput = (totalThroughput / numPeripheral);

    std::cout << "Simulation Results (10s):" << std::endl;
    std::cout << "Packet Delivery Ratio: " << pdr << std::endl;
    std::cout << "Average Delay: " << avgDelay << " s" << std::endl;
    std::cout << "Average Throughput: " << avgThroughput << " Kbps" << std::endl;

    Simulator::Destroy();
    return 0;
}