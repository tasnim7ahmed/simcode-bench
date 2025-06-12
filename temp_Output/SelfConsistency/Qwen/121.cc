#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/dsdv-helper.h"
#include "ns3/drrouting-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/animation-interface.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DisasterRecoveryNetwork");

int main(int argc, char *argv[]) {
    // Enable logging components
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Simulation parameters
    double simulationTime = 60.0; // seconds

    // Create nodes
    NodeContainer centralCommand;
    centralCommand.Create(1);

    NodeContainer medicalUnits;
    medicalUnits.Create(3);

    NodeContainer responderTeams;
    responderTeams.Create(15); // 3 teams of 5 nodes each

    // Combine all nodes for device/container setup
    NodeContainer allNodes = centralCommand;
    allNodes.Add(medicalUnits);
    allNodes.Add(responderTeams);

    // Setup WiFi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac"); // Ad hoc mode
    NetDeviceContainer devices = wifi.Install(phy, mac, allNodes);

    // Enable RTS/CTS
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/RtsCtsThreshold", StringValue("100"));

    // Mobility configuration
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(20.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(centralCommand);
    mobility.Install(medicalUnits);

    // Random mobility for responders
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Value=1.0]"));
    mobility.Install(responderTeams);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP Echo Server and Client Setup
    uint16_t port = 9;
    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    for (uint32_t i = 0; i < 3; ++i) {
        // Each team leader acts as a server
        uint32_t serverIndex = 5 + i * 5; // First node in each team block
        UdpEchoServerHelper server(port);
        serverApps.Add(server.Install(allNodes.Get(serverIndex)));

        // Clients are the rest of the team members
        for (uint32_t j = 0; j < 5; ++j) {
            if (i * 5 + j == serverIndex) continue;
            UdpEchoClientHelper client(interfaces.GetAddress(serverIndex), port);
            client.SetAttribute("MaxPackets", UintegerValue(1000));
            client.SetAttribute("Interval", TimeValue(Seconds(2.0)));
            client.SetAttribute("PacketSize", UintegerValue(512));
            clientApps.Add(client.Install(allNodes.Get(i * 5 + j)));
        }
    }

    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime));

    // Flow Monitor setup
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Animation Interface
    AnimationInterface anim("disaster_recovery_network.xml");
    anim.EnablePacketMetadata(true);

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Output Flow Monitor stats
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow ID: " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Packet Delivery Ratio: " << ((double)i->second.rxPackets / (double)i->second.txPackets) * 100 << "%\n";
        std::cout << "  Total Delay: " << i->second.delaySum.GetSeconds() << "s\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / simulationTime / 1000000 << " Mbps\n";
    }

    Simulator::Destroy();
    return 0;
}