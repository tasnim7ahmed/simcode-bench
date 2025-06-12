#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/spectrum-module.h"

#include <iostream>
#include <string>
#include <map>

using namespace ns3;

struct WifiVersionToStandard {
    std::string wifiVersion;
    WifiMode wifiMode;
    WifiPhyStandard phyStandard;
    FrequencyBand frequencyBand;
};

std::map<std::string, WifiVersionToStandard> wifiVersionMap = {
    {"80211a", {"80211a", WifiMode::OfdmRate6Mbps, WIFI_PHY_STANDARD_80211a, FrequencyBand::BAND_5GHZ}},
    {"80211b", {"80211b", WifiMode::DsssRate1Mbps, WIFI_PHY_STANDARD_80211b, FrequencyBand::BAND_2_4GHZ}},
    {"80211g", {"80211g", WifiMode::OfdmRate6Mbps, WIFI_PHY_STANDARD_80211g, FrequencyBand::BAND_2_4GHZ}},
    {"80211n", {"80211n", WifiMode::HtMcs0, WIFI_PHY_STANDARD_80211n, FrequencyBand::BAND_2_4GHZ}},
    {"80211ac", {"80211ac", WifiMode::VhtMcs0, WIFI_PHY_STANDARD_80211ac, FrequencyBand::BAND_5GHZ}}
};

struct WifiConfiguration {
    std::string wifiVersion;
    std::string rateAdaptationAlgorithm;
};

int main(int argc, char *argv[]) {
    bool enablePcap = false;
    bool enableNetAnim = false;
    bool verbose = false;
    std::string trafficSource = "sta";
    std::string apWifiVersion = "80211ac";
    std::string staWifiVersion = "80211n";
    std::string apRateAdaptation = "Aarf";
    std::string staRateAdaptation = "Aarf";

    CommandLine cmd;
    cmd.AddValue("pcap", "Enable PCAP tracing", enablePcap);
    cmd.AddValue("netanim", "Enable NetAnim animation", enableNetAnim);
    cmd.AddValue("verbose", "Enable verbose output", verbose);
    cmd.AddValue("trafficSource", "Traffic source (ap, sta, both)", trafficSource);
    cmd.AddValue("apWifiVersion", "Wi-Fi version for AP (80211a, 80211b, 80211g, 80211n, 80211ac)", apWifiVersion);
    cmd.AddValue("staWifiVersion", "Wi-Fi version for STA (80211a, 80211b, 80211g, 80211n, 80211ac)", staWifiVersion);
    cmd.AddValue("apRateAdaptation", "Rate adaptation algorithm for AP", apRateAdaptation);
    cmd.AddValue("staRateAdaptation", "Rate adaptation algorithm for STA", staRateAdaptation);
    cmd.Parse(argc, argv);

    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(1);

    WifiConfiguration apConfig;
    apConfig.wifiVersion = apWifiVersion;
    apConfig.rateAdaptationAlgorithm = apRateAdaptation;

    WifiConfiguration staConfig;
    staConfig.wifiVersion = staWifiVersion;
    staConfig.rateAdaptationAlgorithm = staRateAdaptation;

    WifiHelper wifiHelper;
    wifiHelper.SetStandard(wifiVersionMap[apConfig.wifiVersion].phyStandard);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());

    wifiHelper.SetRemoteStationManager(apConfig.rateAdaptationAlgorithm);
    Ssid ssid = Ssid("ns3-wifi");

    WifiMacHelper macHelper;
    macHelper.SetType("ns3::ApWifiMac",
                      "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice = wifiHelper.Install(wifiPhy, macHelper, apNode);

    wifiHelper.SetStandard(wifiVersionMap[staConfig.wifiVersion].phyStandard);
    wifiHelper.SetRemoteStationManager(staConfig.rateAdaptationAlgorithm);
    macHelper.SetType("ns3::StaWifiMac",
                      "Ssid", SsidValue(ssid),
                      "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevice = wifiHelper.Install(wifiPhy, macHelper, staNodes);

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

    InternetStackHelper internet;
    internet.Install(apNode);
    internet.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

    UdpServerHelper server(9);
    ApplicationContainer serverApp = server.Install(apNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpClientHelper client(apInterface.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(1000000));
    client.SetAttribute("Interval", TimeValue(Time("1ms")));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    if (trafficSource == "ap") {
      client.SetRemote(staInterface.GetAddress(0),9);
      clientApps = client.Install(apNode.Get(0));
    } else if (trafficSource == "sta") {
      client.SetRemote(apInterface.GetAddress(0), 9);
      clientApps = client.Install(staNodes.Get(0));
    } else if (trafficSource == "both") {
      client.SetRemote(apInterface.GetAddress(0), 9);
      clientApps = client.Install(staNodes.Get(0));
      UdpClientHelper client2(staInterface.GetAddress(0), 9);
      client2.SetAttribute("MaxPackets", UintegerValue(1000000));
      client2.SetAttribute("Interval", TimeValue(Time("1ms")));
      client2.SetAttribute("PacketSize", UintegerValue(1024));
      client2.SetRemote(apInterface.GetAddress(0), 9);
      ApplicationContainer clientApps2 = client2.Install(apNode.Get(0));
      clientApps2.Start(Seconds(2.0));
      clientApps2.Stop(Seconds(10.0));
    }

    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    if (enablePcap) {
        wifiPhy.EnablePcapAll("wifi-backward-compatibility");
    }

    if (enableNetAnim) {
        AnimationInterface anim("wifi-backward-compatibility.xml");
        anim.SetConstantPosition(apNode.Get(0), 10, 10);
        anim.SetConstantPosition(staNodes.Get(0), 20, 20);
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
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