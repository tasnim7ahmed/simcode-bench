#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-socket-address.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetAodvExample");

int main(int argc, char *argv[])
{
    // Simulation parameters
    uint32_t nNodes = 5;
    double simTime = 30.0;
    double txp = 20.0; // dBm

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(nNodes);

    // Set up Wi-Fi in ad hoc mode
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    wifiPhy.Set("TxPowerStart", DoubleValue(txp));
    wifiPhy.Set("TxPowerEnd", DoubleValue(txp));
    wifiPhy.Set("RxGain", DoubleValue(0));
    wifiPhy.Set("EnergyDetectionThreshold", DoubleValue(-80.0));
    wifiPhy.Set("CcaMode1Threshold", DoubleValue(-85.0));

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility: RandomWaypointModel in a 100m x 100m area
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> allocator = CreateObject<ListPositionAllocator>();
    allocator->Add(Vector(10.0, 10.0, 0.0));
    allocator->Add(Vector(90.0, 10.0, 0.0));
    allocator->Add(Vector(10.0, 90.0, 0.0));
    allocator->Add(Vector(90.0, 90.0, 0.0));
    allocator->Add(Vector(50.0, 50.0, 0.0));
    mobility.SetPositionAllocator(allocator);

    mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                              "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"),
                              "PositionAllocator", PointerValue (allocator),
                              "Bounds", BoxValue (Box (0, 100, 0, 100, 0, 0)));
    mobility.Install(nodes);

    // Trace mobility
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> mobilityStream = ascii.CreateFileStream("manet.mobility");
    mobility.EnableAsciiAll(mobilityStream);

    // Install Internet stack with AODV
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // UDP client/server: node 0 sends to node 4
    uint16_t port = 4000;

    // Install UDP server on node 4
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(4));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    // Install UDP client on node 0
    UdpClientHelper client(interfaces.GetAddress(4), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.2)));
    client.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime));

    // Enable PCAP tracing
    wifiPhy.EnablePcap("manet", devices);

    // Enable Flow Monitor to analyze packet delivery
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Flow monitor statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (auto const& f : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(f.first);
        if ((t.sourceAddress=="10.0.0.1" && t.destinationAddress=="10.0.0.5") ||
            (t.sourceAddress=="10.0.0.5" && t.destinationAddress=="10.0.0.1"))
        {
            std::cout << "Flow " << f.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "  Tx Packets: " << f.second.txPackets << "\n";
            std::cout << "  Rx Packets: " << f.second.rxPackets << "\n";
            std::cout << "  Packet Delivery Ratio: " << (f.second.rxPackets * 100.0 / (double)f.second.txPackets) << "%\n";
            std::cout << "  Tx Bytes:   " << f.second.txBytes << "\n";
            std::cout << "  Rx Bytes:   " << f.second.rxBytes << "\n";
        }
    }

    Simulator::Destroy();
    return 0;
}