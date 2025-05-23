#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <iostream>
#include <map>

int main(int argc, char *argv[])
{
    ns3::Time::SetResolution(ns3::Time::NS);
    double simulationTime = 10.0;
    uint32_t packetSize = 1024;
    uint16_t sinkPort = 9;

    ns3::NodeContainer nodes;
    nodes.Create(3);

    ns3::Ptr<ns3::Node> clientNode = nodes.Get(0);
    ns3::Ptr<ns3::Node> routerNode = nodes.Get(1);
    ns3::Ptr<ns3::Node> serverNode = nodes.Get(2);

    ns3::PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", ns3::StringValue("100Mbps"));
    p2pHelper.SetChannelAttribute("Delay", ns3::StringValue("2ms"));

    ns3::NetDeviceContainer devices01;
    devices01 = p2pHelper.Install(clientNode, routerNode);

    ns3::NetDeviceContainer devices12;
    devices12 = p2pHelper.Install(routerNode, serverNode);

    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    ns3::Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces01;
    interfaces01 = address.Assign(devices01);

    address.SetBase("10.1.2.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces12;
    interfaces12 = address.Assign(devices12);

    ns3::Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    ns3::PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                                     ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), sinkPort));
    ns3::ApplicationContainer serverApp = sinkHelper.Install(serverNode);
    serverApp.Start(ns3::Seconds(0.0));
    serverApp.Stop(ns3::Seconds(simulationTime + 1.0));

    ns3::BulkSendHelper clientHelper("ns3::TcpSocketFactory",
                                     ns3::InetSocketAddress(interfaces12.GetAddress(1), sinkPort));
    clientHelper.SetAttribute("MaxBytes", ns3::UintegerValue(0));
    clientHelper.SetAttribute("SendSize", ns3::UintegerValue(packetSize));

    ns3::ApplicationContainer clientApp = clientHelper.Install(clientNode);
    double clientStartTime = 1.0;
    clientApp.Start(ns3::Seconds(clientStartTime));
    clientApp.Stop(ns3::Seconds(simulationTime));

    p2pHelper.EnablePcapAll("p2p-tcp-simulation");

    ns3::FlowMonitorHelper flowmonHelper;
    ns3::Ptr<ns3::FlowMonitor> flowMonitor = flowmonHelper.InstallAll();

    ns3::Simulator::Stop(ns3::Seconds(simulationTime + 2.0));
    ns3::Simulator::Run();

    flowMonitor->CheckFor = ns3::FlowMonitor::CHECK_ALL;
    flowMonitor->SerializeToXmlFile("p2p-tcp-flowmon.xml", true, true);

    ns3::Ptr<ns3::Ipv4FlowClassifier> classifier = ns3::DynamicCast<ns3::Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    ns3::FlowMonitor::FlowStatsCollection stats = flowMonitor->GetFlowStats();

    double totalThroughput = 0;
    uint64_t totalTxPackets = 0;
    uint64_t totalRxPackets = 0;
    uint64_t totalLostPackets = 0;
    uint64_t totalTxBytes = 0;
    uint64_t totalRxBytes = 0;

    double actualFlowDuration = simulationTime - clientStartTime;
    if (actualFlowDuration <= 0) {
        actualFlowDuration = simulationTime;
    }

    for (std::map<ns3::FlowId, ns3::FlowMonitor::FlowStats>::const_iterator i = stats.Begin(); i != stats.End(); ++i)
    {
        ns3::Ipv4FlowClassifier::FiveTuple t = classifier->GetFiveTuple(i->first);
        if (t.sourceAddress == interfaces01.GetAddress(0) && t.destinationAddress == interfaces12.GetAddress(1))
        {
            totalTxPackets += i->second.txPackets;
            totalRxPackets += i->second.rxPackets;
            totalLostPackets += i->second.lostPackets;
            totalTxBytes += i->second.txBytes;
            totalRxBytes += i->second.rxBytes;

            double throughput = (double)i->second.rxBytes * 8 / (actualFlowDuration * 1000000.0);
            totalThroughput += throughput;
        }
    }

    std::cout << "Total Throughput: " << totalThroughput << " Mbps" << std::endl;
    std::cout << "Total Tx Packets: " << totalTxPackets << std::endl;
    std::cout << "Total Rx Packets: " << totalRxPackets << std::endl;
    std::cout << "Total Lost Packets: " << totalLostPackets << std::endl;
    std::cout << "Total Tx Bytes: " << totalTxBytes << std::endl;
    std::cout << "Total Rx Bytes: " << totalRxBytes << std::endl;
    if (totalTxPackets > 0)
    {
        std::cout << "Packet Loss Rate: " << (double)totalLostPackets / totalTxPackets * 100.0 << "%" << std::endl;
    }
    else
    {
        std::cout << "Packet Loss Rate: 0.0% (No packets transmitted)" << std::endl;
    }

    ns3::Simulator::Destroy();

    return 0;
}