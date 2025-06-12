#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdHocWirelessNetwork");

int main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Setup mobility models
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(10),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(nodes);

    // Setup wifi physical and mac layer
    WifiHelper wifi = WifiHelper::Default();
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("OfdmRate6Mbps"),
                                 "ControlMode", StringValue("OfdmRate6Mbps"));

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Configure transmission power (50 dBm)
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TransmitPowerLevel", DoubleValue(50.0));

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup UDP echo server on both nodes
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Setup UDP echo client applications to send packets in both directions
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(50));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    echoClient = UdpEchoClientHelper(interfaces.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(50));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    clientApps = echoClient.Install(nodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Flow Monitor setup
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
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();

    double totalDelaySum = 0;
    double totalTxPackets = 0;
    double totalRxPackets = 0;
    double totalThroughput = 0;

    for (auto iter = stats.begin(); iter != stats.end(); ++iter)
    {
        FlowId flowId = iter->first;
        FlowMonitor::FlowStats s = iter->second;

        std::cout << "Flow ID: " << flowId << "  ";
        std::cout << "Tx Packets: " << s.txPackets << "  ";
        std::cout << "Rx Packets: " << s.rxPackets << "  ";
        std::cout << "Packet Delivery Ratio: " << ((double)s.rxPackets / (double)s.txPackets) * 100 << "%  ";
        std::cout << "Total Delay: " << s.delaySum.GetSeconds() << "s  ";
        std::cout << "Average Delay: " << (s.delaySum.GetSeconds() / s.rxPackets) << "s  ";
        std::cout << "Throughput: " << s.rxBytes * 8.0 / (s.timeLastRxPacket.GetSeconds() - s.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps" << std::endl;

        totalDelaySum += s.delaySum.GetSeconds();
        totalTxPackets += s.txPackets;
        totalRxPackets += s.rxPackets;
        totalThroughput += s.rxBytes * 8.0 / (s.timeLastRxPacket.GetSeconds() - s.timeFirstTxPacket.GetSeconds()) / 1e6;
    }

    // Average metrics
    if (totalTxPackets > 0)
    {
        std::cout << "\nOverall Packet Delivery Ratio: " << (totalRxPackets / totalTxPackets) * 100 << "%" << std::endl;
        std::cout << "Average End-to-End Delay: " << (totalDelaySum / totalRxPackets) << " seconds" << std::endl;
    }
    std::cout << "Total Throughput: " << totalThroughput << " Mbps" << std::endl;

    Simulator::Destroy();
    return 0;
}