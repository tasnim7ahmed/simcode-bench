#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdHocWirelessNetwork");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure WiFi for ad hoc mode
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                  "DataMode", StringValue("OfdmRate6Mbps"),
                                  "ControlMode", StringValue("OfdmRate6Mbps"));
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Set transmission power to 50.0 dBm
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerEnd", DoubleValue(50.0));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ TxPowerStart", DoubleValue(50.0));

    // Mobility model: random movement within rectangular boundaries (e.g., 100x100)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(100.0),
                                  "DeltaY", DoubleValue(100.0),
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

    // Setup UDP Echo Server on both nodes
    UdpEchoServerHelper echoServer(9); // port 9

    ApplicationContainer serverApps;
    serverApps.Add(echoServer.Install(nodes.Get(0)));
    serverApps.Add(echoServer.Install(nodes.Get(1)));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Setup UDP Echo Client applications sending packets bidirectionally
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(50));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    clientApps.Add(echoClient.Install(nodes.Get(0)));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient2(interfaces.GetAddress(0), 9);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(50));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));

    clientApps.Add(echoClient2.Install(nodes.Get(1)));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Animation output
    AnimationInterface anim("adhoc_network.xml");
    anim.EnablePacketMetadata(true);

    // Flow Monitor setup
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowMonitor = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();

    // Output flow statistics
    flowMonitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();

    for (auto it = stats.begin(); it != stats.end(); ++it) {
        Ipv4FlowClassifier::FiveTuple t = StaticCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier())->FindFlow(it->first);
        std::cout << "Flow ID: " << it->first << " Src Addr: " << t.sourceAddress << " Dst Addr: " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets: " << it->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << it->second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << it->second.lostPackets << std::endl;
        std::cout << "  Packet Delivery Ratio: " << ((double)it->second.rxPackets / (double)it->second.txPackets) << std::endl;
        std::cout << "  Average Delay: " << it->second.delaySum.ToDouble(Time::S) / it->second.rxPackets << "s" << std::endl;
        std::cout << "  Throughput: " << it->second.rxBytes * 8.0 / (it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}