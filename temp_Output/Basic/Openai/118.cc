#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Create 4 nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Set grid position allocator and random walk mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(50.0),
                                 "DeltaY", DoubleValue(50.0),
                                 "GridWidth", UintegerValue(2),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)),
                             "Distance", DoubleValue(10.0),
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                             "Direction", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=6.283185307]"));
    mobility.Install(nodes);

    // Set up YANS wifi channel and physical layer
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("DsssRate11Mbps"),
                                "ControlMode", StringValue("DsssRate1Mbps"));

    WifiMacHelper mac;
    Ssid ssid = Ssid("adhoc-network");
    mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Install Internet stack and set IPv4 addresses
    InternetStackHelper stack;
    stack.Install(nodes);
    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up UDP echo servers on all nodes
    uint16_t port = 9;
    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        UdpEchoServerHelper echoServer(port);
        ApplicationContainer serverApp = echoServer.Install(nodes.Get(i));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(25.0));
        serverApps.Add(serverApp);
    }

    // Each node sends to next node in a ring (0->1->2->3->0)
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        uint32_t next = (i + 1) % nodes.GetN();
        UdpEchoClientHelper echoClient(interfaces.GetAddress(next), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(20));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
        echoClient.SetAttribute("PacketSize", UintegerValue(64));
        ApplicationContainer clientApp = echoClient.Install(nodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(24.0));
        clientApps.Add(clientApp);
    }

    // Set up Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // NetAnim
    AnimationInterface anim("adhoc-animation.xml");

    Simulator::Stop(Seconds(26.0));
    Simulator::Run();

    // Process Flow Monitor stats
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double totalTxPackets = 0, totalRxPackets = 0, totalDelay = 0, totalThroughput = 0;
    uint32_t flows = 0;

    std::cout << "Flow statistics:" << std::endl;
    for (auto iter = stats.begin(); iter != stats.end(); ++iter)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        std::cout << "Flow ID: " << iter->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << iter->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << iter->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << iter->second.lostPackets << "\n";
        if (iter->second.rxPackets > 0)
        {
            double meanDelay = iter->second.delaySum.GetSeconds() / iter->second.rxPackets;
            std::cout << "  Mean Delay: " << meanDelay << " s\n";
            std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1024 << " Kbps\n";
            totalDelay += meanDelay * iter->second.rxPackets;
            totalThroughput += iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds());
        }
        else
        {
            std::cout << "  Mean Delay: N/A\n";
            std::cout << "  Throughput: 0 Kbps\n";
        }
        double deliveryRatio = iter->second.rxPackets * 100.0 / (iter->second.txPackets ? iter->second.txPackets : 1);
        std::cout << "  Packet Delivery Ratio: " << deliveryRatio << " %\n";
        totalTxPackets += iter->second.txPackets;
        totalRxPackets += iter->second.rxPackets;
        ++flows;
    }

    if (flows > 0 && totalRxPackets > 0)
    {
        std::cout << "\n--- Aggregate Network Statistics ---" << std::endl;
        std::cout << "Total Flows: " << flows << std::endl;
        std::cout << "Total Tx Packets: " << totalTxPackets << std::endl;
        std::cout << "Total Rx Packets: " << totalRxPackets << std::endl;
        double totalDeliveryRatio = totalRxPackets * 100.0 / (totalTxPackets ? totalTxPackets : 1);
        std::cout << "Aggregate Packet Delivery Ratio: " << totalDeliveryRatio << " %" << std::endl;
        std::cout << "Aggregate Mean End-to-End Delay: " << (totalDelay / totalRxPackets) << " s" << std::endl;
        std::cout << "Aggregate Throughput: " << (totalThroughput / 1024) << " Kbps" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}