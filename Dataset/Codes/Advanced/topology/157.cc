#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopologyDataset");

void RecordUdpData(Ptr<FlowMonitor> flowMonitor, std::string filename)
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
    LogComponentEnable("RingTopologyDataset", LOG_LEVEL_INFO);

    // Create 4 nodes in a ring topology
    NodeContainer nodes;
    nodes.Create(4);

    // Set up point-to-point links between nodes in a ring
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices.Add(p2p.Install(nodes.Get(0), nodes.Get(1)));
    devices.Add(p2p.Install(nodes.Get(1), nodes.Get(2)));
    devices.Add(p2p.Install(nodes.Get(2), nodes.Get(3)));
    devices.Add(p2p.Install(nodes.Get(3), nodes.Get(0))); // Connect back to node 0 to form a ring

    // Install Internet stack on nodes
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses to devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Setup UDP server on node 0
    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(nodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Setup UDP clients on nodes 1-3 to send data to node 0
    UdpClientHelper udpClient(interfaces.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(100));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < nodes.GetN(); ++i)
    {
        clientApps.Add(udpClient.Install(nodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(9.0));

    // Enable flow monitor
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Record UDP data to a CSV file
    RecordUdpData(flowMonitor, "udp_dataset_ring.csv");

    Simulator::Destroy();
    return 0;
}

