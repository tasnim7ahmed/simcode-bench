#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyDataset");

void RecordTcpData(Ptr<FlowMonitor> flowMonitor, std::string filename)
{
    std::ofstream outFile;
    outFile.open(filename, std::ios::out | std::ios::app);
    outFile << "FlowID,SrcAddr,DestAddr,PacketSize,TxTime,RxTime\n"; // CSV headers

    // Retrieve statistics from flow monitor
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
    for (auto &flow : stats)
    {
        FlowId id = flow.first;
        FlowMonitor::FlowStats flowStats = flow.second;
        Ipv4FlowClassifier::FiveTuple t = flowMonitor->GetClassifier()->FindFlow(id);

        for (uint32_t i = 0; i < flowStats.txPackets; i++)
        {
            outFile << id << ","
                    << t.sourceAddress << ","
                    << t.destinationAddress << ","
                    << flowStats.txBytes << ","
                    << flowStats.timeFirstTxPacket.GetSeconds() << ","
                    << flowStats.timeLastRxPacket.GetSeconds() << "\n";
        }
    }
    outFile.close();
}

int main(int argc, char *argv[])
{
    LogComponentEnable("StarTopologyDataset", LOG_LEVEL_INFO);

    // Create 5 nodes (1 central node and 4 peripheral nodes)
    NodeContainer centralNode;
    centralNode.Create(1);

    NodeContainer peripheralNodes;
    peripheralNodes.Create(4);

    NodeContainer allNodes;
    allNodes.Add(centralNode);
    allNodes.Add(peripheralNodes);

    // Set up point-to-point links between central node and peripheral nodes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    for (uint32_t i = 0; i < peripheralNodes.GetN(); ++i)
    {
        devices.Add(p2p.Install(centralNode.Get(0), peripheralNodes.Get(i)));
    }

    // Install Internet stack on nodes
    InternetStackHelper internet;
    internet.Install(allNodes);

    // Assign IP addresses to devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Setup TCP server on central node (node 0)
    uint16_t port = 8080;
    Address serverAddress(InetSocketAddress(interfaces.GetAddress(0), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApp = packetSinkHelper.Install(centralNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Setup TCP clients on peripheral nodes (nodes 1-4)
    OnOffHelper onOffHelper("ns3::TcpSocketFactory", serverAddress);
    onOffHelper.SetAttribute("DataRate", StringValue("500kbps"));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < peripheralNodes.GetN(); ++i)
    {
        onOffHelper.SetAttribute("Remote", AddressValue(InetSocketAddress(interfaces.GetAddress(0), port)));
        clientApps.Add(onOffHelper.Install(peripheralNodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(9.0));

    // Enable flow monitor
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Record TCP data to a CSV file
    RecordTcpData(flowMonitor, "tcp_dataset.csv");

    Simulator::Destroy();
    return 0;
}

