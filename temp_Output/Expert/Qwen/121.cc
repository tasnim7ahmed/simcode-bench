#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DisasterRecoveryNetwork");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer centralCommand;
    centralCommand.Create(1);

    NodeContainer medicalUnits;
    medicalUnits.Create(3);

    NodeContainer responderTeams;
    responderTeams.Create(3 * 5); // 3 teams of 5 nodes each

    // WiFi setup
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                  "DataMode", StringValue("OfdmRate54Mbps"),
                                  "ControlMode", StringValue("OfdmRate54Mbps"));

    YansWifiPhyHelper phy;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices;
    devices.Add(wifi.Install(phy, mac, centralCommand));
    devices.Add(wifi.Install(phy, mac, medicalUnits));
    devices.Add(wifi.Install(phy, mac, responderTeams));

    // Enable RTS/CTS
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/RtsCtsEnabled", BooleanValue(true));

    // Mobility for responder teams
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(20.0),
                                  "MinY", DoubleValue(20.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(responderTeams);

    // Stationary nodes: central command and medical units
    MobilityHelper stationary;
    stationary.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    stationary.Install(centralCommand);
    stationary.Install(medicalUnits);

    // Internet stack
    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup applications
    uint16_t port = 9;
    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    // One UDP echo server per team, first node in each team acts as server
    for (uint32_t i = 0; i < 3; ++i) {
        uint32_t serverIndex = i * 5;
        UdpEchoServerHelper echoServer(port + i);
        serverApps.Add(echoServer.Install(responderTeams.Get(serverIndex)));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(20.0));
    }

    // Clients from other nodes in the team send to their respective server
    for (uint32_t team = 0; team < 3; ++team) {
        Ipv4Address serverIp = interfaces.GetAddress(team * 5);
        for (uint32_t member = 1; member < 5; ++member) {
            uint32_t nodeIndex = team * 5 + member;
            UdpEchoClientHelper echoClient(serverIp, port + team);
            echoClient.SetAttribute("MaxPackets", UintegerValue(10));
            echoClient.SetAttribute("Interval", TimeValue(Seconds(2.0)));
            echoClient.SetAttribute("PacketSize", UintegerValue(512));
            clientApps.Add(echoClient.Install(responderTeams.Get(nodeIndex)));
        }
    }

    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    // Flow monitor setup
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowMonitor = flowmonHelper.InstallAll();

    // Animation output
    AnimationInterface anim("disaster_recovery_network.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    // Simulation timing
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    // Output flow statistics
    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowStats> stats = flowMonitor->GetFlowStats();

    for (std::map<FlowId, FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow ID: " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")" << std::endl;
        std::cout << "  Tx Packets: " << i->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << i->second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << i->second.lostPackets << std::endl;
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps" << std::endl;
        std::cout << "  Mean Delay: " << i->second.delaySum.GetSeconds() / i->second.rxPackets << " s" << std::endl;
        std::cout << "  Mean Jitter: " << i->second.jitterSum.GetSeconds() / (i->second.rxPackets > 1 ? (i->second.rxPackets - 1) : 1) << " s" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}