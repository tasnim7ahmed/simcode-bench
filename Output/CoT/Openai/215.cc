#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ManetAodvUdpExample");

int main (int argc, char *argv[])
{
    uint32_t numNodes = 4;
    double simTime = 30.0;
    double txPower = 16.0;
    std::string phyMode ("DsssRate11Mbps");

    CommandLine cmd (__FILE__);
    cmd.AddValue ("simTime", "Simulation time (s)", simTime);
    cmd.Parse (argc, argv);

    NodeContainer nodes;
    nodes.Create (numNodes);

    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
        "DataMode", StringValue (phyMode),
        "ControlMode", StringValue (phyMode));

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());
    wifiPhy.Set ("TxPowerStart", DoubleValue (txPower));
    wifiPhy.Set ("TxPowerEnd", DoubleValue (txPower));

    WifiMacHelper wifiMac;
    wifiMac.SetType ("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                              "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"),
                              "PositionAllocator", StringValue (
                                  "ns3::RandomRectanglePositionAllocator["
                                  "X=ns3::UniformRandomVariable[Min=0.0|Max=100.0],"
                                  "Y=ns3::UniformRandomVariable[Min=0.0|Max=100.0]]"));
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue (10.0),
                                  "MinY", DoubleValue (10.0),
                                  "DeltaX", DoubleValue (20.0),
                                  "DeltaY", DoubleValue (20.0),
                                  "GridWidth", UintegerValue (2),
                                  "LayoutType", StringValue ("RowFirst"));
    mobility.Install (nodes);

    InternetStackHelper stack;
    AodvHelper aodv;
    stack.SetRoutingHelper (aodv);
    stack.Install (nodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (devices);

    uint16_t port = 9;
    UdpServerHelper server (port);
    ApplicationContainer serverApp = server.Install (nodes.Get (3));
    serverApp.Start (Seconds (1.0));
    serverApp.Stop (Seconds (simTime));

    UdpClientHelper client (interfaces.GetAddress (3), port);
    client.SetAttribute ("MaxPackets", UintegerValue (10000));
    client.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
    client.SetAttribute ("PacketSize", UintegerValue (512));
    ApplicationContainer clientApp = client.Install (nodes.Get (0));
    clientApp.Start (Seconds (2.0));
    clientApp.Stop (Seconds (simTime - 1));

    wifiPhy.EnablePcapAll ("manet-aodv");

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();

    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
    uint64_t rxPackets = 0;
    uint64_t txPackets = 0;
    uint64_t rxBytes = 0;
    double throughput = 0.0;

    for (auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);
        if (t.destinationAddress == interfaces.GetAddress (3))
        {
            txPackets += flow.second.txPackets;
            rxPackets += flow.second.rxPackets;
            rxBytes += flow.second.rxBytes;
            if (flow.second.timeLastRxPacket.GetSeconds () > 0)
            {
                double interval = flow.second.timeLastRxPacket.GetSeconds () - flow.second.timeFirstTxPacket.GetSeconds ();
                if (interval > 0)
                    throughput += (rxBytes * 8.0) / (interval * 1e6); // Mbps
            }
        }
    }

    std::cout << "========== Simulation Results ==========\n";
    std::cout << "Transmitted Packets: " << txPackets << std::endl;
    std::cout << "Received Packets   : " << rxPackets << std::endl;
    std::cout << "Packet Loss        : " << txPackets - rxPackets << std::endl;
    std::cout << "Throughput (Mbps)  : " << throughput << std::endl;
    std::cout << "========================================\n";

    Simulator::Destroy ();
    return 0;
}