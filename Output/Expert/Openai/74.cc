#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/internet-module.h"
#include "ns3/dsdv-helper.h"
#include "ns3/aodv-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/dsr-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/config-store-module.h"
#include <fstream>
#include <iomanip>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetRoutingComparison");

enum RoutingProtocolType
{
    PROT_DSDV,
    PROT_AODV,
    PROT_OLSR,
    PROT_DSR
};

std::string GetProtocolName(RoutingProtocolType prot)
{
    switch (prot)
    {
    case PROT_DSDV: return "DSDV";
    case PROT_AODV: return "AODV";
    case PROT_OLSR: return "OLSR";
    case PROT_DSR: return "DSR";
    default: return "Unknown";
    }
}

int main(int argc, char *argv[])
{
    uint32_t numNodes = 50;
    double simTime = 200.0;
    double nodeSpeed = 20.0;
    double areaWidth = 1500.0;
    double areaHeight = 300.0;
    double txPower = 7.5;
    RoutingProtocolType proto = PROT_DSDV;
    uint32_t numFlows = 10;
    std::string csvFile = "manet-results.csv";
    bool enableFlowMonitor = true;
    bool enableMobilityTracing = false;

    CommandLine cmd;
    cmd.AddValue("numNodes", "Number of nodes", numNodes);
    cmd.AddValue("simTime", "Simulation time (s)", simTime);
    cmd.AddValue("nodeSpeed", "Node speed (m/s)", nodeSpeed);
    cmd.AddValue("areaWidth", "Area width (m)", areaWidth);
    cmd.AddValue("areaHeight", "Area height (m)", areaHeight);
    cmd.AddValue("txPower", "Transmission power (dBm)", txPower);
    cmd.AddValue("protocol", "Routing protocol (DSDV, AODV, OLSR, DSR)", proto);
    cmd.AddValue("numFlows", "Number of UDP flows", numFlows);
    cmd.AddValue("csvFile", "CSV output file", csvFile);
    cmd.AddValue("enableFlowMonitor", "Enable Flow Monitor", enableFlowMonitor);
    cmd.AddValue("enableMobilityTracing", "Enable Mobility Tracing", enableMobilityTracing);

    std::string protoStr = "DSDV";
    cmd.AddValue("protoStr", "Routing protocol as string", protoStr);

    cmd.Parse(argc, argv);

    if (protoStr == "DSDV") proto = PROT_DSDV;
    else if (protoStr == "AODV") proto = PROT_AODV;
    else if (protoStr == "OLSR") proto = PROT_OLSR;
    else if (protoStr == "DSR") proto = PROT_DSR;

    SeedManager::SetSeed(1);

    NodeContainer nodes;
    nodes.Create(numNodes);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(nodeSpeed) + "]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0]"),
                              "PositionAllocator", PointerValue(CreateObjectWithAttributes<RandomRectanglePositionAllocator>(
                                                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaWidth) + "]"),
                                                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaHeight) + "]"))));
    mobility.Install(nodes);

    if (enableMobilityTracing)
    {
        mobility.EnableAsciiAll(CreateObject<AsciiTraceHelper>()->CreateFileStream("mobility-trace.tr"));
    }

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    wifiPhy.Set("TxPowerStart", DoubleValue(txPower));
    wifiPhy.Set("TxPowerEnd", DoubleValue(txPower));
    wifiPhy.Set("RxGain", DoubleValue(0));
    wifiPhy.Set("TxGain", DoubleValue(0));
    wifiPhy.Set("EnergyDetectionThreshold", DoubleValue(-101));
    wifiPhy.Set("CcaMode1Threshold", DoubleValue(-101));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    InternetStackHelper internet;
    DsdvHelper dsdv;
    AodvHelper aodv;
    OlsrHelper olsr;
    DsrHelper dsr;
    DsrMainHelper dsrMain;
    switch (proto)
    {
    case PROT_DSDV:
        internet.SetRoutingHelper(dsdv);
        internet.Install(nodes);
        break;
    case PROT_AODV:
        internet.SetRoutingHelper(aodv);
        internet.Install(nodes);
        break;
    case PROT_OLSR:
        internet.SetRoutingHelper(olsr);
        internet.Install(nodes);
        break;
    case PROT_DSR:
        internet.Install(nodes);
        dsrMain.Install(dsr, nodes);
        break;
    }

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    uint16_t port = 8000;

    ApplicationContainer sourceApps;
    ApplicationContainer sinkApps;

    Ptr<UniformRandomVariable> startVar = CreateObject<UniformRandomVariable>();
    startVar->SetAttribute("Min", DoubleValue(50.0));
    startVar->SetAttribute("Max", DoubleValue(51.0));

    std::vector<std::pair<uint32_t, uint32_t>> flowPairs;
    std::set<uint32_t> usedSrc, usedDst;
    Ptr<UniformRandomVariable> pairSelector = CreateObject<UniformRandomVariable>();

    while (flowPairs.size() < numFlows)
    {
        uint32_t src = pairSelector->GetInteger(0, numNodes - 1);
        uint32_t dst = pairSelector->GetInteger(0, numNodes - 1);
        if (src != dst)
        {
            // Optional: Avoid duplicate src/dst for better fairness
            flowPairs.emplace_back(src, dst);
        }
    }

    for (uint32_t i = 0; i < numFlows; ++i)
    {
        uint32_t src = flowPairs[i].first;
        uint32_t dst = flowPairs[i].second;

        PacketSinkHelper sink("ns3::UdpSocketFactory",
                              InetSocketAddress(interfaces.GetAddress(dst), port + i));
        ApplicationContainer sinkApp = sink.Install(nodes.Get(dst));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(simTime));
        sinkApps.Add(sinkApp);

        OnOffHelper onoff("ns3::UdpSocketFactory",
                          InetSocketAddress(interfaces.GetAddress(dst), port + i));
        onoff.SetAttribute("PacketSize", UintegerValue(512));
        onoff.SetAttribute("DataRate", DataRateValue(DataRate("1024kbps")));
        double startTime = startVar->GetValue();
        onoff.SetAttribute("StartTime", TimeValue(Seconds(startTime)));
        onoff.SetAttribute("StopTime", TimeValue(Seconds(simTime - 0.1)));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
        ApplicationContainer sourceApp = onoff.Install(nodes.Get(src));
        sourceApps.Add(sourceApp);
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor;
    if (enableFlowMonitor)
    {
        monitor = flowmon.InstallAll();
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    std::ofstream outFile(csvFile, std::ios_base::out);
    outFile << "Protocol,NumNodes,NodeSpeed,Area,TxPower,NumFlows,FlowId,SrcAddr,SrcPort,DestAddr,DestPort,TxPackets,TxBytes,RxPackets,RxBytes,LostPackets,Throughput_bps,Delay_ms,Jitter_ms,SimulationTime\n";

    if (enableFlowMonitor && monitor)
    {
        monitor->CheckForLostPackets();
        Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
        std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
        for (const auto &flow : stats)
        {
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
            double throughput = flow.second.rxBytes * 8.0 / (simTime - 50.0);
            double avgDelay = (flow.second.rxPackets > 0) ? (flow.second.delaySum.GetSeconds() / flow.second.rxPackets * 1000.0) : 0.0;
            double avgJitter = (flow.second.rxPackets > 0) ? (flow.second.jitterSum.GetSeconds() / flow.second.rxPackets * 1000.0) : 0.0;
            outFile << GetProtocolName(proto) << ","
                    << numNodes << ","
                    << nodeSpeed << ","
                    << "\"" << areaWidth << "x" << areaHeight << "\","
                    << txPower << ","
                    << numFlows << ","
                    << flow.first << ","
                    << t.sourceAddress << ","
                    << t.sourcePort << ","
                    << t.destinationAddress << ","
                    << t.destinationPort << ","
                    << flow.second.txPackets << ","
                    << flow.second.txBytes << ","
                    << flow.second.rxPackets << ","
                    << flow.second.rxBytes << ","
                    << flow.second.lostPackets << ","
                    << throughput << ","
                    << avgDelay << ","
                    << avgJitter << ","
                    << simTime
                    << std::endl;
        }
        monitor->SerializeToXmlFile("flowmon.xml", true, true);
    }
    outFile.close();

    Simulator::Destroy();
    return 0;
}