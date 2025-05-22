I
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DatasetExample");

void RecordPacketData(Ptr<FlowMonitor> flowMonitor, std::string filename)
{
    std::ofstream outFile;
    outFile.open(filename, std::ios::out | std::ios::app);
    outFile << "FlowID,SrcAddr,DestAddr,PacketSize,TxTime,RxTime\n";  // CSV headers

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
    LogComponentEnable("DatasetExample", LOG_LEVEL_INFO);

    // Create 5 nodes
    NodeContainer nodes;
    nodes.Create(5);

    // Configure WiFi channel and physical layer
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Set up the WiFi MAC layer for Ad-hoc networking
    WifiHelper wifi;
    WifiMacHelper mac;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Set up mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install the internet stack and assign IP addresses
    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Setup UDP Echo server on node 4 (central server)
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(4));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Setup UDP Echo client on nodes 0, 1, 2, and 3 to send traffic to node 4
    for (int i = 0; i < 4; i++)
    {
        UdpEchoClientHelper echoClient(interfaces.GetAddress(4), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(10));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = echoClient.Install(nodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
    }

    // Enable PCAP tracing
    phy.EnablePcapAll("dataset-example");

    // Flow monitor to capture packet statistics
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Record the dataset to a CSV file
    RecordPacketData(flowMonitor, "packet_dataset.csv");

    Simulator::Destroy();
    return 0;
}

