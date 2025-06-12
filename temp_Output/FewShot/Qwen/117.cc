#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure WiFi in ad hoc mode
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Set transmission power to 50 dBm
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<WifiNetDevice> device = DynamicCast<WifiNetDevice>(devices.Get(i));
        if (device) {
            device->GetPhy()->SetTxPowerStart(50.0);
            device->GetPhy()->SetTxPowerEnd(50.0);
        }
    }

    // Mobility model: random movement within a rectangle (100x100)
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

    // Install Internet stack and assign IP addresses
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup UDP Echo Servers on both nodes
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);

    ApplicationContainer serverApps0 = echoServer.Install(nodes.Get(0));
    serverApps0.Start(Seconds(1.0));
    serverApps0.Stop(Seconds(25.0));

    ApplicationContainer serverApps1 = echoServer.Install(nodes.Get(1));
    serverApps1.Start(Seconds(1.0));
    serverApps1.Stop(Seconds(25.0));

    // Setup UDP Echo Clients on both nodes sending to each other
    UdpEchoClientHelper echoClient0(interfaces.GetAddress(1), port);
    echoClient0.SetAttribute("MaxPackets", UintegerValue(50));
    echoClient0.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    echoClient0.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp0 = echoClient0.Install(nodes.Get(0));
    clientApp0.Start(Seconds(2.0));
    clientApp0.Stop(Seconds(25.0));

    UdpEchoClientHelper echoClient1(interfaces.GetAddress(0), port);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(50));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp1 = echoClient1.Install(nodes.Get(1));
    clientApp1.Start(Seconds(2.0));
    clientApp1.Stop(Seconds(25.0));

    // Flow monitor setup
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowMonitor = flowmonHelper.InstallAll();

    // Animation output
    AnimationInterface anim("wireless_adhoc_network.xml");

    // Run simulation
    Simulator::Stop(Seconds(25.0));
    Simulator::Run();

    // Output flow statistics
    flowMonitor->CheckForLostPackets();
    std::cout << "\n\nFlow Monitor Statistics\n";
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();
    for (auto it = stats.begin(); it != stats.end(); ++it) {
        std::cout << "Flow ID: " << it->first << " (" 
                  << interfaces.GetAddress(it->second.txBytes > 0 ? 0 : 1) 
                  << " -> " 
                  << interfaces.GetAddress(it->second.rxBytes > 0 ? 1 : 0) 
                  << ")\n";
        std::cout << "  Tx Packets: " << it->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << it->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << it->second.lostPackets << "\n";
        std::cout << "  Packet Delivery Ratio: " 
                  << ((double)it->second.rxPackets / (double)it->second.txPackets) * 100.0 << "%\n";
        std::cout << "  Average Delay: " << it->second.delaySum.GetSeconds() / it->second.rxPackets << "s\n";
        std::cout << "  Throughput: " 
                  << (it->second.rxBytes * 8.0) / (it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n\n";
    }

    Simulator::Destroy();
    return 0;
}