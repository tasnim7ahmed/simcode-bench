#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

// Function to convert Wifi standard string to enum
WifiStandard GetWifiStandardFromString(const std::string& wifiStandardString) {
    if (wifiStandardString == "80211a") {
        return WIFI_PHY_STANDARD_80211a;
    } else if (wifiStandardString == "80211b") {
        return WIFI_PHY_STANDARD_80211b;
    } else if (wifiStandardString == "80211g") {
        return WIFI_PHY_STANDARD_80211g;
    } else if (wifiStandardString == "80211n") {
        return WIFI_PHY_STANDARD_80211n;
    } else if (wifiStandardString == "80211ac") {
        return WIFI_PHY_STANDARD_80211ac;
    } else if (wifiStandardString == "80211ax") {
        return WIFI_PHY_STANDARD_80211ax;
    } else if (wifiStandardString == "80211be") {
        return WIFI_PHY_STANDARD_80211be;
    } else {
        return WIFI_PHY_STANDARD_80211_UNSPECIFIED;
    }
}

// Function to determine physical band based on Wifi standard
WifiPhyBand GetWifiPhyBandFromStandard(WifiStandard wifiStandard) {
    switch (wifiStandard) {
        case WIFI_PHY_STANDARD_80211a:
            return WIFI_PHY_BAND_5GHZ;
        case WIFI_PHY_STANDARD_80211b:
            return WIFI_PHY_BAND_2_4GHZ;
        case WIFI_PHY_STANDARD_80211g:
            return WIFI_PHY_BAND_2_4GHZ;
        case WIFI_PHY_STANDARD_80211n:
            return WIFI_PHY_BAND_2_4GHZ;
        case WIFI_PHY_STANDARD_80211ac:
            return WIFI_PHY_BAND_5GHZ;
        case WIFI_PHY_STANDARD_80211ax:
            return WIFI_PHY_BAND_5GHZ;
        case WIFI_PHY_STANDARD_80211be:
            return WIFI_PHY_BAND_6GHZ;
        default:
            return WIFI_PHY_BAND_UNSPECIFIED;
    }
}

int main(int argc, char* argv[]) {
    bool enableRtsCts = false;
    std::string rateAdaptationAlgorithm = "ns3::AarfWifiManager";
    std::string accessPointWifiStandard = "80211a";
    std::string stationWifiStandard = "80211n";
    bool verbose = false;
    bool tracing = false;
    bool pcap = false;

    CommandLine cmd;
    cmd.AddValue("EnableRtsCts", "Enable or disable RTS/CTS mechanism", enableRtsCts);
    cmd.AddValue("RateAdaptationAlgorithm", "Rate Adaptation Algorithm", rateAdaptationAlgorithm);
    cmd.AddValue("AccessPointWifiStandard", "Wifi Standard for the Access Point", accessPointWifiStandard);
    cmd.AddValue("StationWifiStandard", "Wifi Standard for the Station", stationWifiStandard);
    cmd.AddValue("verbose", "Tell application to log if set to true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.AddValue("pcap", "Enable pcap output on all interfaces", pcap);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue(enableRtsCts ? "0" : "2200"));

    NodeContainer apNode;
    apNode.Create(1);

    NodeContainer staNodes;
    staNodes.Create(1);

    // Set up Wifi
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(GetWifiStandardFromString(accessPointWifiStandard));

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Set Physical Band for AP
    phy.Set("ChannelNumber", UintegerValue(36));
    phy.Set("Frequency", UintegerValue(5180));
    phy.Set("Bandwidth", UintegerValue(20));

    // Configure AP MAC layer
    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(0.1)));

    NetDeviceContainer apDevice = wifiHelper.Install(phy, mac, apNode);

    // Configure STA
    wifiHelper.SetStandard(GetWifiStandardFromString(stationWifiStandard));
    phy.Set("Bandwidth", UintegerValue(20));
    phy.Set("ChannelNumber", UintegerValue(36));
    phy.Set("Frequency", UintegerValue(5180));

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer staDevice = wifiHelper.Install(phy, mac, staNodes);

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
    mobility.Install(staNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

    // UDP server on AP
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(apNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP client on STA
    UdpEchoClientHelper echoClient(apInterface.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(staNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Populate routing table
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));

    // Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    if (tracing) {
        phy.EnablePcapAll("wifi-example");
    }

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