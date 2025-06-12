#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool enableNetAnim = true;
    double simulationTime = 60.0;

    CommandLine cmd;
    cmd.AddValue("enableNetAnim", "Enable NetAnim visualization", enableNetAnim);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    // Create nodes: APs and STAs
    NodeContainer apNodes;
    apNodes.Create(3);
    NodeContainer staNodes;
    staNodes.Create(6);
    NodeContainer serverNode;
    serverNode.Create(1);

    // Configure Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    // AP configuration
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(MilliSeconds(100)));

    NetDeviceContainer apDevices = wifi.Install(phy, mac, apNodes);

    // STA configuration
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);
    stack.Install(serverNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer serverInterface = address.Assign(serverNode);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                  "X", StringValue("0.0"),
                                  "Y", StringValue("0.0"),
                                  "Z", StringValue("0.0"),
                                  "Rho", StringValue("50.0"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-100, 100, -100, 100)));
    mobility.Install(staNodes);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);
    mobility.Install(serverNode);

    // Distribute STAs across APs
    for (uint32_t i = 0; i < staNodes.GetN(); ++i) {
        Ptr<Node> staNode = staNodes.Get(i);
        if (i % 3 == 0) {
            mobility.SetPosition(staNode, Vector(0.0, 0.0, 0.0));
        } else if (i % 3 == 1) {
            mobility.SetPosition(staNode, Vector(10.0, 10.0, 0.0));
        } else {
            mobility.SetPosition(staNode, Vector(20.0, 20.0, 0.0));
        }
    }

    // UDP traffic: STAs to Server
    uint16_t port = 9;
    UdpClientHelper client(serverInterface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(10000));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    client.SetAttribute("PacketSize", UintegerValue(1024);

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < staNodes.GetN(); ++i) {
        clientApps.Add(client.Install(staNodes.Get(i)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime - 1));

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(serverNode.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    // Enable NetAnim
    if (enableNetAnim) {
        AnimationInterface anim("wifi-animation.xml");
        anim.SetConstantPosition(serverNode.Get(0), 50.0, 50.0);
    }

    // Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Print flow statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
    }

    Simulator::Destroy();
    return 0;
}