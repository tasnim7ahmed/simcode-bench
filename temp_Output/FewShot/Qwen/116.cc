#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/animation-interface.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create two nodes for ad hoc network
    NodeContainer nodes;
    nodes.Create(2);

    // Configure WiFi in ad hoc mode
    WifiMacHelper wifiMac;
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211a);
    wifiHelper.SetRemoteStationManager("ns3::ArfWifiManager");

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifiHelper.Install(wifiMac, nodes);

    // Set up mobility models with random movement within a 100x100 area
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(50.0),
                                  "DeltaY", DoubleValue(50.0),
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

    // Set up UDP Echo Servers on both nodes
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps[2];
    serverApps[0] = echoServer.Install(nodes.Get(0));
    serverApps[0]->SetStartTime(Seconds(0.0));
    serverApps[0]->SetStopTime(Seconds(10.0));

    serverApps[1] = echoServer.Install(nodes.Get(1));
    serverApps[1]->SetStartTime(Seconds(0.0));
    serverApps[1]->SetStopTime(Seconds(10.0));

    // Set up UDP Echo Clients on both nodes (bidirectional)
    ApplicationContainer clientApps[2];

    UdpEchoClientHelper echoClient1(interfaces.GetAddress(1), port);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(50));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
    clientApps[0] = echoClient1.Install(nodes.Get(0));
    clientApps[0]->SetStartTime(Seconds(1.0));
    clientApps[0]->SetStopTime(Seconds(10.0));

    UdpEchoClientHelper echoClient2(interfaces.GetAddress(0), port);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(50));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
    clientApps[1] = echoClient2.Install(nodes.Get(1));
    clientApps[1]->SetStartTime(Seconds(1.0));
    clientApps[1]->SetStopTime(Seconds(10.0));

    // Enable flow monitoring
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowMonitor = flowmonHelper.InstallAll();

    // Generate animation XML file
    AnimationInterface anim("wireless-adhoc-network.xml");
    anim.EnablePacketMetadata(true);

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Output flow statistics
    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets: " << i->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << i->second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << i->second.lostPackets << std::endl;
        std::cout << "  Packet Delivery Ratio: " << ((double)i->second.rxPackets / (double)i->second.txPackets) << std::endl;
        std::cout << "  Average Delay: " << i->second.delaySum.ToDouble(Time::S) / i->second.rxPackets << "s" << std::endl;
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}