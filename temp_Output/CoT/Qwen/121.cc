#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DisasterRecoveryNetwork");

int main(int argc, char *argv[]) {
    uint32_t simDuration = 60;
    double distance = 50.0; // Distance between central command and medical units

    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer centralCommand;
    centralCommand.Create(1);

    NodeContainer medicalUnits;
    medicalUnits.Create(3);

    NodeContainer responderTeams;
    responderTeams.Create(15); // 3 teams of 5 nodes each

    NodeContainer allNodes;
    allNodes.Add(centralCommand);
    allNodes.Add(medicalUnits);
    allNodes.Add(responderTeams);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHz);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));
    
    Ssid ssid = Ssid("disaster-recovery");
    mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer devices = wifi.Install(phy, mac, allNodes);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(centralCommand);
    mobility.Install(medicalUnits);

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-100, 100, -100, 100)),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    mobility.Install(responderTeams);

    InternetStackHelper stack;
    stack.Install(allNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Enable RTS/CTS
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/RtsCtsEnabled", BooleanValue(true));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/RtsCtsThreshold", UintegerValue(100));

    // UDP Echo Server Setup for each team
    uint16_t port = 9;
    ApplicationContainer serverApps;

    for (uint32_t i = 0; i < 3; ++i) {
        uint32_t leaderIndex = i * 5; // Assume first node in each team is the leader
        UdpEchoServerHelper echoServer(port);
        serverApps.Add(echoServer.Install(allNodes.Get(leaderIndex)));
    }

    serverApps.Start(Seconds(1.0));
    serverAds.Stop(Seconds(simDuration));

    // UDP Clients
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < 15; ++i) {
        if (i % 5 != 0) { // Only non-leader nodes send traffic
            UdpEchoClientHelper echoClient(interfaces.GetAddress(i - (i % 5)), port);
            echoClient.SetAttribute("Interval", TimeValue(Seconds(2.0)));
            echoClient.SetAttribute("PacketSize", UintegerValue(512));
            clientApps.Add(echoClient.Install(allNodes.Get(i)));
        }
    }

    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simDuration));

    // Flow Monitor setup
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Animation output
    AnimationInterface anim("disaster_recovery.xml");
    anim.EnablePacketMetadata(true);
    anim.EnableIpv4RouteTracking("disaster_recovery_route.xml", Seconds(0), Seconds(simDuration), Seconds(0.5));

    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets: " << i->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << i->second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << i->second.lostPackets << std::endl;
        std::cout << "  Packet Delivery Ratio: " << ((double)i->second.rxPackets / (double)i->second.txPackets) << std::endl;
        std::cout << "  Average Delay: " << i->second.delaySum.GetSeconds() / i->second.rxPackets << std::endl;
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}