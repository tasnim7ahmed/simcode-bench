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

    NodeContainer centralNode;
    centralNode.Create(1);

    NodeContainer peripheralNodes;
    peripheralNodes.Create(4);

    NodeContainer allNodes;
    allNodes.Add(centralNode);
    allNodes.Add(peripheralNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devices[4];
    for (uint32_t i = 0; i < 4; ++i) {
        NodeContainer pair(centralNode.Get(0), peripheralNodes.Get(i));
        devices[i] = p2p.Install(pair);

        std::ostringstream oss;
        oss << "star-p2p-" << i << ".pcap";
        p2p.EnablePcap(oss.str(), devices[i], true);
    }

    InternetStackHelper stack;
    stack.Install(allNodes);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[4];
    for (uint32_t i = 0; i < 4; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i+1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = address.Assign(devices[i]);
    }

    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(centralNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    for (uint32_t i = 0; i < 4; ++i) {
        UdpEchoClientHelper echoClient(interfaces[i].GetAddress(0), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(10));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApps = echoClient.Install(peripheralNodes.Get(i));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));
    }

    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowMonitor = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    double totalTxPackets = 0;
    double totalRxPackets = 0;
    double totalDelay = 0.0;
    double totalThroughput = 0.0;
    uint32_t countedFlows = 0;

    for (const auto& flow : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        if (t.destinationPort == port) {
            totalTxPackets += flow.second.txPackets;
            totalRxPackets += flow.second.rxPackets;
            totalDelay += flow.second.delaySum.GetSeconds();
            double duration = (flow.second.timeLastRxPacket - flow.second.timeFirstTxPacket).GetSeconds();
            if (duration > 0 && flow.second.rxBytes > 0) {
                totalThroughput += (flow.second.rxBytes * 8.0) / duration;
                countedFlows += 1;
            }
        }
    }

    double pdr = totalRxPackets / totalTxPackets * 100.0;
    double avgDelay = totalRxPackets != 0 ? totalDelay / totalRxPackets : 0.0; // seconds
    double avgThroughput = countedFlows != 0 ? totalThroughput / countedFlows : 0.0; // bps

    std::cout << "Packet Delivery Ratio: " << pdr << "%" << std::endl;
    std::cout << "Average End-to-End Delay: " << avgDelay * 1000.0 << " ms" << std::endl;
    std::cout << "Average Throughput: " << avgThroughput / 1e6 << " Mbps" << std::endl;

    Simulator::Destroy();
    return 0;
}