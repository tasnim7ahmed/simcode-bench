#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Simulation duration
    double simDuration = 100.0;
    bool enableFlowMonitor = true;
    bool enableRoutingTablePrint = false;
    double routingTableInterval = 5.0;
    std::string animFile = "ad-hoc-aodv-animation.xml";

    // Command line parameters
    CommandLine cmd;
    cmd.AddValue("simDuration", "Total simulation time (seconds)", simDuration);
    cmd.AddValue("enableFlowMonitor", "Enable Flow Monitor to collect stats", enableFlowMonitor);
    cmd.AddValue("enableRoutingTablePrint", "Enable printing of routing tables", enableRoutingTablePrint);
    cmd.AddValue("routingTableInterval", "Interval for printing routing tables", routingTableInterval);
    cmd.AddValue("animFile", "Output file for NetAnim XML tracing", animFile);
    cmd.Parse(argc, argv);

    // Enable AODV logging if needed
    LogComponentEnable("AodvRoutingProtocol", LOG_LEVEL_LOGIC);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes container and create 10 nodes
    NodeContainer nodes;
    nodes.Create(10);

    // Setup mobility in a grid with random movement
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(20.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(nodes);

    // Install wireless devices using YansWifiPhy and WifiMac
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Install internet stack with AODV
    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv);
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup UDP Echo Server on each node
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        serverApps = echoServer.Install(nodes.Get(i));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(simDuration));
    }

    // Setup UDP Echo Clients sending packets circularly every 4 seconds
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        uint32_t nextNode = (i + 1) % nodes.GetN();
        UdpEchoClientHelper echoClient(interfaces.GetAddress(nextNode), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF)); // Infinite
        echoClient.SetAttribute("Interval", TimeValue(Seconds(4.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));
        clientApps = echoClient.Install(nodes.Get(i));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(simDuration - 1.0));
    }

    // Enable PCAP tracing
    wifiPhy.EnablePcapAll("aodv-adhoc-network");

    // Print routing tables periodically if enabled
    if (enableRoutingTablePrint) {
        aodv.PrintRoutingTableAllAt(Seconds(routingTableInterval), nodes);
        Simulator::Stop(Seconds(simDuration));
    }

    // Set up flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor;
    if (enableFlowMonitor) {
        monitor = flowmon.InstallAll();
    }

    // Set up animation
    AnimationInterface anim(animFile);
    anim.EnablePacketMetadata(true);

    // Run the simulation
    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();

    // Output flow statistics
    if (enableFlowMonitor) {
        flowmon.SerializeToXmlFile("aodv-flow-stats.xml", false, false);
        monitor->CheckForLostPackets();
        Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
        std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
        for (auto it = stats.begin(); it != stats.end(); ++it) {
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
            std::cout << "Flow ID: " << it->first << " Src Addr: " << t.sourceAddress << " Dst Addr: " << t.destinationAddress << std::endl;
            std::cout << "  Tx Packets: " << it->second.txPackets << std::endl;
            std::cout << "  Rx Packets: " << it->second.rxPackets << std::endl;
            std::cout << "  Lost Packets: " << it->second.lostPackets << std::endl;
            std::cout << "  Throughput: " << it->second.rxBytes * 8.0 / (it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds()) / 1024 << " Kbps" << std::endl;
            std::cout << "  Mean Delay: " << it->second.delaySum.GetSeconds() / it->second.rxPackets << " s" << std::endl;
        }
    }

    Simulator::Destroy();
    return 0;
}