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
    bool verbose = true;
    uint32_t numResponderTeams = 3;
    uint32_t nodesPerTeam = 5;
    double simTime = 60.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Turn on log components", verbose);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    NodeContainer centralCommand;
    centralCommand.Create(1);

    NodeContainer medicalUnits;
    medicalUnits.Create(3);

    NodeContainer responderTeams[numResponderTeams];
    for (uint32_t i = 0; i < numResponderTeams; ++i) {
        responderTeams[i].Create(nodesPerTeam);
    }

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHz);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HtMcs7"),
                                 "ControlMode", StringValue("HtMcs0"));

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("disaster-recovery-network");
    mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer devices;
    devices.Add(wifi.Install(phy, mac, centralCommand));
    devices.Add(wifi.Install(phy, mac, medicalUnits));
    for (uint32_t i = 0; i < numResponderTeams; ++i) {
        devices.Add(wifi.Install(phy, mac, responderTeams[i]));
    }

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(10),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomDirection2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2]"));
    
    // Stationary nodes: central command and medical units
    mobility.Install(centralCommand);
    mobility.Install(medicalUnits);

    // Mobile responders
    for (uint32_t i = 0; i < numResponderTeams; ++i) {
        mobility.Install(responderTeams[i]);
    }

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces;
    interfaces = address.Assign(devices);

    // Enable RTS/CTS
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/RtsCts", BooleanValue(true));

    uint16_t port = 9;

    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    for (uint32_t t = 0; t < numResponderTeams; ++t) {
        // Server on first node of each team
        UdpEchoServerHelper echoServer(port);
        serverApps.Add(echoServer.Install(responderTeams[t].Get(0)));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(simTime));

        // Clients on other nodes in the team
        for (uint32_t n = 1; n < nodesPerTeam; ++n) {
            UdpEchoClientHelper echoClient(interfaces.GetAddress(t * nodesPerTeam + 0), port);
            echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
            echoClient.SetAttribute("Interval", TimeValue(Seconds(2.0)));
            echoClient.SetAttribute("PacketSize", UintegerValue(512));

            clientApps.Add(echoClient.Install(responderTeams[t].Get(n)));
            clientApps.Start(Seconds(2.0));
            clientApps.Stop(Seconds(simTime));
        }
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    AsciiTraceHelper ascii;
    phy.EnableAsciiAll(ascii.CreateFileStream("disaster-recovery.tr"));
    phy.EnablePcapAll("disaster-recovery");

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AnimationInterface anim("disaster-recovery.xml");

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow ID: " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Packet Delivery Ratio: " << ((double)i->second.rxPackets / (double)i->second.txPackets) << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
        std::cout << "  End-to-End Delay Mean: " << i->second.delaySum.GetSeconds() / i->second.rxPackets << " s\n";
    }

    Simulator::Destroy();
    return 0;
}