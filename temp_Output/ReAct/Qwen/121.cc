#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DisasterRecoveryNetwork");

int main(int argc, char *argv[]) {
    uint32_t simDuration = 60; // seconds

    // Create nodes
    NodeContainer centralCommand;
    centralCommand.Create(1);

    NodeContainer medicalUnits;
    medicalUnits.Create(3);

    NodeContainer responderTeams;
    responderTeams.Create(3 * 5); // 3 teams of 5 nodes each

    NodeContainer allNodes;
    allNodes.Add(centralCommand);
    allNodes.Add(medicalUnits);
    allNodes.Add(responderTeams);

    // Setup WiFi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    WifiMacHelper mac;
    Ssid ssid = Ssid("disaster-recovery-network");
    mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));

    YansWifiPhyHelper phy;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    NetDeviceContainer devices = wifi.Install(phy, mac, allNodes);

    // Mobility for responders
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(responderTeams);

    // Stationary for command and medical units
    MobilityHelper stationary;
    stationary.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    stationary.Install(centralCommand);
    stationary.Install(medicalUnits);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Enable RTS/CTS
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/Configuration/$ns3::WifiPhy/SupportCtsToSelf", BooleanValue(true));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Configuration/$ns3::WifiMac/UseRtsForUnicast", BooleanValue(true));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Configuration/$ns3::WifiMac/RtsCtsThreshold", UintegerValue(100));

    // UDP Echo server setup per team
    uint16_t port = 9;
    std::vector<ApplicationContainer> servers;
    std::vector<ApplicationContainer> clients;

    for (uint32_t team = 0; team < 3; ++team) {
        uint32_t leaderIndex = team * 5; // first node in each team as the server
        Ipv4Address serverIp = interfaces.GetAddress(leaderIndex + centralCommand.GetN() + medicalUnits.GetN());

        // Server app
        UdpEchoServerHelper server(port);
        ApplicationContainer serverApp = server.Install(responderTeams.Get(leaderIndex));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(simDuration));
        servers.push_back(serverApp);

        // Clients in the same team sending to their server
        for (uint32_t member = 1; member < 5; ++member) {
            uint32_t clientIndex = team * 5 + member;
            UdpEchoClientHelper client(serverIp, port);
            client.SetAttribute("MaxPackets", UintegerValue(1000));
            client.SetAttribute("Interval", TimeValue(Seconds(2.0)));
            client.SetAttribute("PacketSize", UintegerValue(1024));

            ApplicationContainer clientApp = client.Install(responderTeams.Get(clientIndex));
            clientApp.Start(Seconds(2.0));
            clientApp.Stop(Seconds(simDuration));
            clients.push_back(clientApp);
        }
    }

    // Flow monitor setup
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowMonitor = flowmonHelper.InstallAll();

    // Animation
    AnimationInterface anim("disaster_recovery_network.xml");
    anim.EnablePacketMetadata(true);

    // Routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();

    // Output flow stats
    flowMonitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = dynamic_cast<Ipv4FlowClassifier*>(flowmonHelper.GetClassifier())->FindFlow(iter->first);
        std::cout << "Flow ID: " << iter->first 
                  << " Src Addr: " << t.sourceAddress 
                  << " Dst Addr: " << t.destinationAddress
                  << " Packet Delivery Ratio: " << (double)iter->second.rxPackets / (double)iter->second.txPackets
                  << " Avg Delay: " << iter->second.delaySum.GetSeconds() / iter->second.rxPackets
                  << " Throughput: " << (iter->second.rxBytes * 8.0) / (Simulator::Now().GetSeconds()) / 1000.0 << " Kbps" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}