#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/animation-interface.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Global configuration
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("100"));

    // Create nodes: 1 command node + 3 medical units + 3 teams * 5 responders = 21 nodes
    NodeContainer commandNode;
    commandNode.Create(1);

    NodeContainer medicalUnits;
    medicalUnits.Create(3);

    NodeContainer responderTeams[3];
    for (int i = 0; i < 3; ++i) {
        responderTeams[i].Create(5);
    }

    // Combine all nodes into a single container
    NodeContainer allNodes;
    allNodes.Add(commandNode);
    allNodes.Add(medicalUnits);
    for (int i = 0; i < 3; ++i) {
        allNodes.Add(responderTeams[i]);
    }

    // WiFi configuration
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHz);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));

    YansWifiPhyHelper phy;
    phy.SetErrorRateModel("ns3::NistErrorRateModel");

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    // Channel setup
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    NetDeviceContainer devices = wifi.Install(phy, mac, allNodes);

    // Mobility model
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(commandNode);
    mobility.Install(medicalUnits);

    // Random movement for responders
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)),
                              "Speed", StringValue("ns3::UniformRandomVariable[Min=0.5,Max=1.5]"));
    for (int i = 0; i < 3; ++i) {
        mobility.Install(responderTeams[i]);
    }

    // Internet stack and IP addresses
    InternetStackHelper stack;
    stack.Install(allNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP Echo Servers on each team's first node
    uint16_t port = 9;
    std::vector<ApplicationContainer> serverApps;
    std::vector<ApplicationContainer> clientApps;

    for (int teamIndex = 0; teamIndex < 3; ++teamIndex) {
        UdpEchoServerHelper echoServer(port);
        ApplicationContainer serverApp = echoServer.Install(responderTeams[teamIndex].Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(30.0));
        serverApps.push_back(serverApp);
    }

    // UDP Clients from other nodes in the team sending to their team's server
    for (int teamIndex = 0; teamIndex < 3; ++teamIndex) {
        Ipv4Address serverIp = interfaces.GetAddress(teamIndex * 5);  // Server is first node of each team

        for (uint32_t memberIndex = 1; memberIndex < 5; ++memberIndex++) {
            UdpEchoClientHelper echoClient(serverIp, port);
            echoClient.SetAttribute("MaxPackets", UintegerValue(10));
            echoClient.SetAttribute("Interval", TimeValue(Seconds(2.0)));
            echoClient.SetAttribute("PacketSize", UintegerValue(512));

            ApplicationContainer clientApp = echoClient.Install(responderTeams[teamIndex].Get(memberIndex));
            clientApp.Start(Seconds(2.0));
            clientApp.Stop(Seconds(30.0));
            clientApps.push_back(clientApp);
        }
    }

    // Flow Monitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowMonitor = flowmonHelper.Install(allNodes);

    // Animation Interface
    AnimationInterface anim("disaster_recovery_network.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    // Simulation run
    Simulator::Stop(Seconds(30.0));
    Simulator::Run();

    // Output flow statistics
    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        std::cout << "Flow ID: " << iter->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets: " << iter->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << iter->second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << iter->second.lostPackets << std::endl;
        std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps" << std::endl;
        std::cout << "  Mean Delay: " << iter->second.delaySum.GetSeconds() / iter->second.rxPackets << " s" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}