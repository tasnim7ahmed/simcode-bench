#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NetworkSimulation");

int main(int argc, char *argv[]) {
    uint32_t lanNodes = 5;
    Time simulationTime = Seconds(10.0);

    NodeContainer nodesLAN;
    nodesLAN.Create(lanNodes);

    NodeContainer routers;
    routers.Create(3);

    NodeContainer r1 = routers.Get(0);
    NodeContainer r2 = routers.Get(1);
    NodeContainer r3 = routers.Get(2);

    NodeContainer p2pLink1(routers.Get(0), routers.Get(1));
    NodeContainer p2pLink2(routers.Get(1), routers.Get(2));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevices1 = p2p.Install(p2pLink1);
    NetDeviceContainer p2pDevices2 = p2p.Install(p2pLink2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(0.01)));

    NetDeviceContainer lanDevices = csma.Install(nodesLAN);

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces1 = address.Assign(p2pDevices1);

    address.NewNetwork();
    Ipv4InterfaceContainer p2pInterfaces2 = address.Assign(p2pDevices2);

    address.NewNetwork();
    Ipv4InterfaceContainer lanInterfaces = address.Assign(lanDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(lanInterfaces.GetAddress(1), sinkPort));

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodesLAN.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(simulationTime);

    OnOffHelper client("ns3::TcpSocketFactory", sinkAddress);
    client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    client.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(r3);
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(simulationTime - Seconds(1.0));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AsciiTraceHelper asciiTraceHelper;
    p2p.EnableAsciiAll(asciiTraceHelper.CreateFileStream("network-simulation.tr"));
    csma.EnableAsciiAll(asciiTraceHelper.CreateFileStream("network-simulation-lan.tr"));

    Simulator::Stop(simulationTime);
    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::ofstream outFile("results.csv");
    outFile << "FlowID,Source,Destination,PacketsSent,ReceivedPackets,Throughput" << std::endl;

    for (auto it = stats.begin(); it != stats.end(); ++it) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
        std::stringstream protoStream;
        protoStream << (uint32_t)t.protocol;
        std::string proto = protoStream.str();

        if (t.protocol == 6) { // TCP
            outFile << it->first << ","
                    << t.sourceAddress << ":" << t.sourcePort << ","
                    << t.destinationAddress << ":" << t.destinationPort << ","
                    << it->second.txPackets << ","
                    << it->second.rxPackets << ","
                    << it->second.rxBytes * 8.0 / (it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds()) / 1000000 << std::endl;
        }
    }

    outFile.close();
    Simulator::Destroy();
    return 0;
}