#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/spectrum-module.h"
#include <fstream>
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SpatialReuseSimulation");

int main(int argc, char* argv[]) {
    bool verbose = false;
    double simulationTime = 10;
    std::string dataRate = "OfdmRate6MbpsBW20MHz";
    double distance1 = 10;
    double distance2 = 15;
    double distance3 = 20;
    double txPower = 20;
    int packetSize = 1024;
    double interval = 0.01;
    bool enableObssPd = true;
    double obssPdThreshold = -82;
    double ccaEdThreshold = -85;
    int channelWidth = 20; //MHz

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if true", verbose);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("dataRate", "Data rate", dataRate);
    cmd.AddValue("distance1", "Distance STA1-AP1", distance1);
    cmd.AddValue("distance2", "Distance STA2-AP2", distance2);
    cmd.AddValue("distance3", "Distance AP1-STA2", distance3);
    cmd.AddValue("txPower", "Transmit power (dBm)", txPower);
    cmd.AddValue("packetSize", "Size of packets in bytes", packetSize);
    cmd.AddValue("interval", "Interval between packets in seconds", interval);
    cmd.AddValue("enableObssPd", "Enable OBSS PD", enableObssPd);
    cmd.AddValue("obssPdThreshold", "OBSS PD threshold (dBm)", obssPdThreshold);
    cmd.AddValue("ccaEdThreshold", "CCA ED threshold (dBm)", ccaEdThreshold);
    cmd.AddValue("channelWidth", "Channel width (MHz)", channelWidth);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("SpatialReuseSimulation", LOG_LEVEL_INFO);
    }

    NodeContainer apNodes;
    apNodes.Create(2);

    NodeContainer staNodes;
    staNodes.Create(2);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());
    phy.Set("TxPowerStart", DoubleValue(txPower));
    phy.Set("TxPowerEnd", DoubleValue(txPower));
    phy.Set("CcaEdThreshold", DoubleValue(ccaEdThreshold));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211ax);

    WifiMacHelper mac;

    Ssid ssid1 = Ssid("ns-3-ssid-1");
    Ssid ssid2 = Ssid("ns-3-ssid-2");

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid1),
                "BeaconGeneration", BooleanValue(true),
                "BeaconInterval", TimeValue(Seconds(0.1)));

    NetDeviceContainer apDevice1 = wifi.Install(phy, mac, apNodes.Get(0));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid2),
                "BeaconGeneration", BooleanValue(true),
                "BeaconInterval", TimeValue(Seconds(0.1)));

    NetDeviceContainer apDevice2 = wifi.Install(phy, mac, apNodes.Get(1));

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid1),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice1 = wifi.Install(phy, mac, staNodes.Get(0));

     mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid2),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice2 = wifi.Install(phy, mac, staNodes.Get(1));

    if (enableObssPd) {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/EnergyDetectionThresholdObss", DoubleValue(obssPdThreshold));
    }

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(apNodes);
    mobility.Install(staNodes);

    Ptr<Node> ap1 = apNodes.Get(0);
    Ptr<Node> ap2 = apNodes.Get(1);
    Ptr<Node> sta1 = staNodes.Get(0);
    Ptr<Node> sta2 = staNodes.Get(1);

    ap1->GetObject<MobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));
    ap2->GetObject<MobilityModel>()->SetPosition(Vector(distance3, 0.0, 0.0));
    sta1->GetObject<MobilityModel>()->SetPosition(Vector(distance1, 5.0, 0.0));
    sta2->GetObject<MobilityModel>()->SetPosition(Vector(distance3 + distance2, 5.0, 0.0));


    InternetStackHelper internet;
    internet.Install(apNodes);
    internet.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface1 = address.Assign(apDevice1);
    Ipv4InterfaceContainer staInterface1 = address.Assign(staDevice1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface2 = address.Assign(apDevice2);
    Ipv4InterfaceContainer staInterface2 = address.Assign(staDevice2);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    UdpClientHelper client1(apInterface1.GetAddress(0), 9);
    client1.SetAttribute("Interval", TimeValue(Seconds(interval)));
    client1.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps1 = client1.Install(staNodes.Get(0));
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(simulationTime));

    UdpClientHelper client2(apInterface2.GetAddress(0), 9);
    client2.SetAttribute("Interval", TimeValue(Seconds(interval)));
    client2.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps2 = client2.Install(staNodes.Get(1));
    clientApps2.Start(Seconds(2.0));
    clientApps2.Stop(Seconds(simulationTime));

    UdpServerHelper server1(9);
    ApplicationContainer serverApps1 = server1.Install(apNodes.Get(0));
    serverApps1.Start(Seconds(1.0));
    serverApps1.Stop(Seconds(simulationTime));

    UdpServerHelper server2(9);
    ApplicationContainer serverApps2 = server2.Install(apNodes.Get(1));
    serverApps2.Start(Seconds(1.0));
    serverApps2.Stop(Seconds(simulationTime));

    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 1));

    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double throughput1 = 0.0;
    double throughput2 = 0.0;

    for (auto i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);

        if (t.destinationAddress == apInterface1.GetAddress(0)) {
            throughput1 = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
             std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
            std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
            std::cout << "  Throughput: " << throughput1 << " Mbps\n";

        } else if (t.destinationAddress == apInterface2.GetAddress(0)) {
             throughput2 = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
             std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
            std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
            std::cout << "  Throughput: " << throughput2 << " Mbps\n";
        }
    }

    std::cout << "Throughput BSS1: " << throughput1 << " Mbps" << std::endl;
    std::cout << "Throughput BSS2: " << throughput2 << " Mbps" << std::endl;

    Simulator::Destroy();
    return 0;
}