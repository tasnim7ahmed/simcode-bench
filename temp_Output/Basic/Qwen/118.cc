#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdHocWirelessNetwork");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Setup Wi-Fi in ad hoc mode
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate6Mbps"), "ControlMode", StringValue("OfdmRate6Mbps"));

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility model: grid layout with random movement
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(20.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(nodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IPv4 addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Setup UDP echo server on each node
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        serverApps.Add(echoServer.Install(nodes.Get(i)));
    }
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Setup UDP echo clients in ring topology
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        uint32_t nextNodeIndex = (i + 1) % nodes.GetN();
        Ipv4Address nextNodeIp = interfaces.GetAddress(nextNodeIndex, 0);

        UdpEchoClientHelper echoClient(nextNodeIp, 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(20));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        clientApps.Add(echoClient.Install(nodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Setup global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Flow monitor setup
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowMonitor = flowmonHelper.InstallAll();

    // Animation output
    AnimationInterface anim("adhoc_network.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    // Run simulation
    Simulator::Stop(Seconds(11.0));
    Simulator::Run();

    // Output flow statistics
    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        std::cout << "Flow ID: " << iter->first << " Src Addr: " << t.sourceAddress << " Dst Addr: " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets: " << iter->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << iter->second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << iter->second.lostPackets << std::endl;
        std::cout << "  Packet Delivery Ratio: " << ((double)iter->second.rxPackets / (double)iter->second.txPackets) << std::endl;
        std::cout << "  Average Delay: " << iter->second.delaySum.GetSeconds() / iter->second.rxPackets << " s" << std::endl;
        std::cout << "  Throughput: " << (iter->second.rxBytes * 8.0) / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}