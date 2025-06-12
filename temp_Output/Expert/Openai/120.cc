#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/wifi-module.h"
#include <iomanip>
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocAodvGridEchoFlowMon");

void
PrintRoutingTableAll(std::string when)
{
    Ptr<OutputStreamWrapper> stream = Create<OutputStreamWrapper>(&std::cout);
    AodvHelper aodv;
    for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i)
    {
        Ptr<Node> node = *i;
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        Ptr<Ipv4RoutingProtocol> protocol = ipv4->GetRoutingProtocol();
        Ptr<aodv::RoutingProtocol> aodvProtocol = DynamicCast<aodv::RoutingProtocol>(protocol);
        if (aodvProtocol)
        {
            *stream->GetStream() << "Routing table at " << when
                                << " for node " << node->GetId() << ":\n";
            aodvProtocol->PrintRoutingTable(stream);
        }
    }
}

int
main(int argc, char *argv[])
{
    uint32_t nNodes = 10;
    double gridWidth = 100.0;
    double gridHeight = 100.0;
    double simTime = 60.0;
    bool printRoutingTables = true;
    double printInterval = 15.0;
    std::string animFile = "aodv-adhoc-grid.xml";

    CommandLine cmd(__FILE__);
    cmd.AddValue("printRoutingTables", "Enable printing of routing tables", printRoutingTables);
    cmd.AddValue("printInterval", "Interval (s) for printing routing tables", printInterval);
    cmd.AddValue("simTime", "Total simulation time (s)", simTime);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(nNodes);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    GridPositionAllocatorHelper gridAlloc;
    gridAlloc.SetMinX(0.0);
    gridAlloc.SetMinY(0.0);
    gridAlloc.SetDeltaX(gridWidth / 3);
    gridAlloc.SetDeltaY(gridHeight / 3);
    gridAlloc.SetGridWidth(4);
    gridAlloc.SetLayoutType(GridPositionAllocatorHelper::ROW_FIRST);
    Ptr<PositionAllocator> positionAlloc = gridAlloc.CreatePositionAllocator();
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=4.0]"),
                             "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
                             "PositionAllocator", PointerValue(positionAlloc));
    mobility.Install(nodes);

    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv);
    stack.Install(nodes);

    Ipv4AddressHelper ip;
    ip.SetBase("10.1.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaces = ip.Assign(devices);

    uint16_t echoPort = 9;
    ApplicationContainer servers, clients;

    for (uint32_t i = 0; i < nNodes; ++i)
    {
        UdpEchoServerHelper echoServer(echoPort);
        servers.Add(echoServer.Install(nodes.Get(i)));
    }

    servers.Start(Seconds(0.5));
    servers.Stop(Seconds(simTime));

    for (uint32_t i = 0; i < nNodes; ++i)
    {
        uint32_t next = (i + 1) % nNodes;
        UdpEchoClientHelper echoClient(ifaces.GetAddress(next), echoPort);
        echoClient.SetAttribute("MaxPackets", UintegerValue(10000));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(4.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(64));

        ApplicationContainer clientApp = echoClient.Install(nodes.Get(i));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(simTime));
        clients.Add(clientApp);
    }

    wifiPhy.EnablePcapAll("aodv-adhoc-grid");

    if (printRoutingTables)
    {
        for (double t = printInterval; t < simTime; t += printInterval)
        {
            Simulator::Schedule(Seconds(t), &PrintRoutingTableAll, std::string("t=") + std::to_string(int(t)));
        }
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AnimationInterface anim(animFile);

    Simulator::Stop(Seconds(simTime + 1));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    uint32_t totalTxPackets = 0, totalRxPackets = 0;
    double totalDelay = 0.0;
    uint64_t totalRxBytes = 0;
    double firstTxTime = simTime, lastRxTime = 0.0;

    for (auto const &flow : stats)
    {
        totalTxPackets += flow.second.txPackets;
        totalRxPackets += flow.second.rxPackets;
        totalDelay += flow.second.delaySum.GetSeconds();
        totalRxBytes += flow.second.rxBytes;
        if (flow.second.timeFirstTxPacket.GetSeconds() < firstTxTime)
            firstTxTime = flow.second.timeFirstTxPacket.GetSeconds();
        if (flow.second.timeLastRxPacket.GetSeconds() > lastRxTime)
            lastRxTime = flow.second.timeLastRxPacket.GetSeconds();
    }

    double pdr = (totalTxPackets > 0) ? double(totalRxPackets) / totalTxPackets * 100.0 : 0.0;
    double avgDelay = (totalRxPackets > 0) ? totalDelay / totalRxPackets : 0.0;
    double throughput = (lastRxTime > firstTxTime) ?
        (totalRxBytes * 8.0) / (lastRxTime - firstTxTime) / 1000.0 : 0.0;

    std::cout << std::setprecision(4);
    std::cout << "\n==== Flow Monitor Summary ====" << std::endl;
    std::cout << "Total Tx Packets:    " << totalTxPackets << std::endl;
    std::cout << "Total Rx Packets:    " << totalRxPackets << std::endl;
    std::cout << "Packet Delivery Ratio: " << pdr << " %" << std::endl;
    std::cout << "Average End-to-End Delay: " << avgDelay << " s" << std::endl;
    std::cout << "Throughput:         " << throughput << " kbps" << std::endl;
    std::cout << "=============================\n" << std::endl;

    Simulator::Destroy();
    return 0;
}