#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiHandoverSimulation");

int main(int argc, char *argv[]) {
    bool enableNetAnim = true;
    std::string phyMode = "DsssRate1Mbps";
    uint32_t numNodes = 6;
    double simulationTime = 20;
    double distance = 10.0;

    CommandLine cmd;
    cmd.AddValue("enableNetAnim", "Enable NetAnim visualization", enableNetAnim);
    cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
    cmd.AddValue("numNodes", "Number of mobile nodes", numNodes);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));
    Config::SetGlobal("RngRun", UintegerValue(1));

    NodeContainer apNodes;
    apNodes.Create(2);

    NodeContainer mobileNodes;
    mobileNodes.Create(numNodes);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());

    QosWifiMacHelper wifiMac;
    Ssid ssid1 = Ssid("ns-3-ssid-1");
    Ssid ssid2 = Ssid("ns-3-ssid-2");

    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid1),
                    "BeaconInterval", TimeValue(MilliSeconds(100)));

    NetDeviceContainer apDevice1 = wifi.Install(wifiPhy, wifiMac, apNodes.Get(0));

    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid2),
                    "BeaconInterval", TimeValue(MilliSeconds(100)));

    NetDeviceContainer apDevice2 = wifi.Install(wifiPhy, wifiMac, apNodes.Get(1));

    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid1),
                    "ActiveProbing", BooleanValue(false));

    NetDeviceContainer mobileDevice1 = wifi.Install(wifiPhy, wifiMac, mobileNodes.Get(0));

    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid2),
                    "ActiveProbing", BooleanValue(false));

        NetDeviceContainer mobileDevicesRemaining;
    for(uint32_t i = 1; i < numNodes; ++i) {
        mobileDevicesRemaining.Add(wifi.Install(wifiPhy, wifiMac, mobileNodes.Get(i)));
    }


    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                  "X", StringValue("100.0"),
                                  "Y", StringValue("100.0"),
                                  "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=30.0]"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Mode", StringValue("Time"),
                              "Time", StringValue("1s"),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                              "Bounds", StringValue("150x150"));

    mobility.Install(mobileNodes);

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(50.0),
                                  "MinY", DoubleValue(50.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(distance),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.Install(apNodes);

    InternetStackHelper internet;
    internet.Install(apNodes);
    internet.Install(mobileNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface1 = ipv4.Assign(apDevice1);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface2 = ipv4.Assign(apDevice2);


    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer mobileInterface1 = ipv4.Assign(mobileDevice1);

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer mobileInterfacesRemaining = ipv4.Assign(mobileDevicesRemaining);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(apNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime - 1));

    UdpEchoClientHelper echoClient(apInterface1.GetAddress(0), 9);
    echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(mobileNodes);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime - 2));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    if (enableNetAnim) {
        AnimationInterface anim("wifi-handover.xml");
        anim.SetConstantPosition(apNodes.Get(0), 50, 50);
        anim.SetConstantPosition(apNodes.Get(1), 50, 60);
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

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

    monitor->SerializeToXmlFile("wifi-handover.flowmon", true, true);

    Simulator::Destroy();
    return 0;
}