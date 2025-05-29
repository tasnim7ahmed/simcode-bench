#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdHocAodvNetAnimExample");

int main (int argc, char *argv[])
{
    // Simulation parameters
    uint32_t numNodes = 4;
    double simTime = 30.0;
    double areaX = 100.0;
    double areaY = 100.0;
    double udpStartTime = 2.0;
    double udpEndTime = simTime - 1;

    // Create nodes
    NodeContainer nodes;
    nodes.Create (numNodes);

    // Set up wifi physical and mac layer (Adhoc)
    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());

    WifiMacHelper wifiMac;
    wifiMac.SetType ("ns3::AdhocWifiMac");

    NetDeviceContainer devices;
    devices = wifi.Install (wifiPhy, wifiMac, nodes);

    // Install Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                               "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                               "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"),
                               "PositionAllocator", 
                               PointerValue(CreateObjectWithAttributes<RandomRectanglePositionAllocator> (
                                   "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                   "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]")
                               )));
    mobility.Install (nodes);

    // Install Internet Stack with AODV
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper (aodv);
    internet.Install (nodes);

    // Assign IP Addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

    // UDP Server/Client: set up for all pairs i->j (i != j)
    uint16_t port = 4000;
    ApplicationContainer serverApps, clientApps;
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        UdpServerHelper server (port + i);
        serverApps.Add (server.Install (nodes.Get (i)));
    }
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (simTime));

    for (uint32_t src = 0; src < numNodes; ++src)
    {
        for (uint32_t dst = 0; dst < numNodes; ++dst)
        {
            if (src == dst) continue;
            UdpClientHelper client (interfaces.GetAddress (dst), port + dst);
            client.SetAttribute ("MaxPackets", UintegerValue (320));
            client.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
            client.SetAttribute ("PacketSize", UintegerValue (512));
            clientApps.Add (client.Install (nodes.Get (src)));
        }
    }
    clientApps.Start (Seconds (udpStartTime));
    clientApps.Stop (Seconds (udpEndTime));

    // FlowMonitor
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    // NetAnim setup
    AnimationInterface anim ("adhoc_aodv_netanim.xml");
    anim.SetMaxPktsPerTraceFile(500000);
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        anim.UpdateNodeDescription (i, "Node" + std::to_string(i));
        anim.UpdateNodeColor (i, 0, 255, 0); // Green
    }
    anim.EnablePacketMetadata (true);

    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();

    // Collect FlowMonitor Results
    double rxPackets = 0;
    double txPackets = 0;
    double rxBytes = 0;
    double txBytes = 0;
    double delaySum = 0;
    double delayPackets = 0;
    double firstTx = 0.0, lastRx = 0.0;

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowHelper.GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats ();
    for (const auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);
        if (t.destinationPort >= port && t.destinationPort < port + numNodes)
        {
            txPackets += flow.second.txPackets;
            rxPackets += flow.second.rxPackets;
            txBytes += flow.second.txBytes;
            rxBytes += flow.second.rxBytes;
            if (flow.second.rxPackets > 0)
            {
                delaySum += flow.second.delaySum.GetSeconds ();
                delayPackets += flow.second.rxPackets;
            }
            if (flow.second.timeFirstTxPacket.GetSeconds () > 0.0 && (firstTx == 0.0 || flow.second.timeFirstTxPacket.GetSeconds () < firstTx))
                firstTx = flow.second.timeFirstTxPacket.GetSeconds ();
            if (flow.second.timeLastRxPacket.GetSeconds () > lastRx)
                lastRx = flow.second.timeLastRxPacket.GetSeconds ();
        }
    }
    double pdr = (txPackets > 0) ? (rxPackets / txPackets) * 100.0 : 0.0;
    double throughput = (rxBytes * 8.0) / (lastRx - firstTx) / 1e3; // kbps
    double avgDelay = (delayPackets > 0) ? (delaySum / delayPackets) : 0.0;

    std::cout << "========== Simulation Results ==========" << std::endl;
    std::cout << "Packet Delivery Ratio    : " << pdr << " %" << std::endl;
    std::cout << "Throughput              : " << throughput << " kbps" << std::endl;
    std::cout << "Average End-to-End Delay: " << avgDelay << " s" << std::endl;

    flowMonitor->SerializeToXmlFile ("adhoc_aodv_flowmon.xml", true, true);

    Simulator::Destroy ();
    return 0;
}