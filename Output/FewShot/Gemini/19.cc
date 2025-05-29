#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

struct WifiConfiguration {
    std::string standard;
    std::string phyMode;
    WifiMode wifiMode;
    uint32_t dataRate;
};

WifiConfiguration GetWifiConfiguration(std::string wifiStandard) {
    WifiConfiguration config;
    if (wifiStandard == "80211a") {
        config.standard = "80211a";
        config.phyMode = "OfdmRate6Mbps";
        config.wifiMode = WifiMode("OfdmRate6Mbps");
        config.dataRate = 6000000;
    } else if (wifiStandard == "80211b") {
        config.standard = "80211b";
        config.phyMode = "DsssRate1Mbps";
        config.wifiMode = WifiMode("DsssRate1Mbps");
        config.dataRate = 1000000;
    } else if (wifiStandard == "80211g") {
        config.standard = "80211g";
        config.phyMode = "OfdmRate6Mbps";
        config.wifiMode = WifiMode("OfdmRate6Mbps");
        config.dataRate = 6000000;
    } else if (wifiStandard == "80211n") {
        config.standard = "80211n";
        config.phyMode = "HtMcs0";
        config.wifiMode = WifiMode("HtMcs0");
        config.dataRate = 6500000;
    } else if (wifiStandard == "80211ac") {
        config.standard = "80211ac";
        config.phyMode = "VhtMcs0";
        config.wifiMode = WifiMode("VhtMcs0");
        config.dataRate = 6500000;
    } else {
        config.standard = "80211a";
        config.phyMode = "OfdmRate6Mbps";
        config.wifiMode = WifiMode("OfdmRate6Mbps");
        config.dataRate = 6000000;
    }
    return config;
}

int main(int argc, char *argv[]) {
    bool enableRtsCts = false;
    std::string rateAdaptationAlgorithm = "AarfWifiManager";
    std::string APWifiStandard = "80211g";
    std::string STAWifiStandard = "80211ac";

    CommandLine cmd;
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
    cmd.AddValue("rateAdaptationAlgorithm", "Rate Adaptation Algorithm", rateAdaptationAlgorithm);
    cmd.AddValue("APWifiStandard", "Wi-Fi standard for AP", APWifiStandard);
    cmd.AddValue("STAWifiStandard", "Wi-Fi standard for STA", STAWifiStandard);
    cmd.Parse(argc, argv);

    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(1);

    WifiHelper wifi;

    // AP Configuration
    WifiConfiguration apConfig = GetWifiConfiguration(APWifiStandard);
    wifi.SetStandard(apConfig.standard);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
    wifiChannel.AddPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiPhy.SetChannel(wifiChannel.Create());

    wifi.SetRemoteStationManager(rateAdaptationAlgorithm);

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns3-wifi");
    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, apNode);

    // STA Configuration
    WifiConfiguration staConfig = GetWifiConfiguration(STAWifiStandard);
    wifi.SetStandard(staConfig.standard);

    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid));

    NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, staNodes);

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(staNodes);

    // Internet Stack
    InternetStackHelper internet;
    internet.Install(apNode);
    internet.Install(staNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = ipv4.Assign(apDevice);
    Ipv4InterfaceContainer staInterface = ipv4.Assign(staDevices);

    // UDP Server and Client
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(apNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(apInterface.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024);

    ApplicationContainer clientApp = echoClient.Install(staNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
    }

    Simulator::Destroy();
    return 0;
}