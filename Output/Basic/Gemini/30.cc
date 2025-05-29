#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config-store-module.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTosSimulation");

int main(int argc, char *argv[]) {
    bool verbose = false;
    uint32_t nSta = 3;
    uint32_t htMcs = 7;
    uint32_t channelWidth = 20;
    bool shortGuardInterval = true;
    double distance = 5.0;
    bool rtsCts = false;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if true", verbose);
    cmd.AddValue("nSta", "Number of wifi STA devices", nSta);
    cmd.AddValue("htMcs", "HT data rate MCS", htMcs);
    cmd.AddValue("channelWidth", "Channel width (20 or 40 MHz)", channelWidth);
    cmd.AddValue("shortGuardInterval", "Use short guard interval", shortGuardInterval);
    cmd.AddValue("distance", "Distance between AP and STAs", distance);
    cmd.AddValue("rtsCts", "Enable RTS/CTS", rtsCts);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    Config::SetDefault("ns3::WifiMacQueue::MaxPackets", UintegerValue(1000));
    Config::SetDefault("ns3::WifiMacQueue::MaxDelay", TimeValue(MilliSeconds(100)));

    NodeContainer staNodes;
    staNodes.Create(nSta);
    NodeContainer apNode;
    apNode.Create(1);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns-3-ssid");
    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, staNodes);

    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid),
                    "BeaconInterval", TimeValue(MilliSeconds(100)));

    NetDeviceContainer apDevices = wifi.Install(wifiPhy, wifiMac, apNode);

    if (rtsCts) {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("0"));
    } else {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));
    }

    std::stringstream ss;
    ss << "HtMcs" << htMcs;
    std::string rate = ss.str();

    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode",
                         StringValue(rate));

    HtWifiMacHelper htWifiMacHelper;

    if (channelWidth == 40) {
        htWifiMacHelper.SetChannelWidth(WifiChannelWidth::WIDTH_40);
    }

    if (shortGuardInterval) {
        htWifiMacHelper.SetShortGuardIntervalEnabled(true);
    } else {
        htWifiMacHelper.SetShortGuardIntervalEnabled(false);
    }

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(nSta),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNodes);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);

    Ptr<MobilityModel> apLoc = apNode.Get(0)->GetObject<MobilityModel>();
    Vector3 pos = Vector3(0.0, 0.0, 0.0);
    apLoc->SetPosition(pos);

    InternetStackHelper internet;
    internet.Install(apNode);
    internet.Install(staNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i = ipv4.Assign(apDevices);
    Ipv4InterfaceContainer j = ipv4.Assign(staDevices);

    Ipv4Address serverAddress = i.GetAddress(0);
    uint16_t port = 9;

    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(apNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpClientHelper client(serverAddress, port);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295));
    client.SetAttribute("Interval", TimeValue(MicroSeconds(10)));
    client.SetAttribute("PacketSize", UintegerValue(1472));

    ApplicationContainer clientApps;
    for (uint32_t k = 0; k < nSta; ++k) {
      clientApps.Add(client.Install(staNodes.Get(k)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(11.0));

    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double totalRxBytes = 0.0;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if (t.destinationAddress == serverAddress) {
            totalRxBytes += i->second.rxBytes;
        }
    }

    double throughput = (totalRxBytes * 8.0) / (9.0 * 1e6);

    std::cout << "Aggregated throughput: " << throughput << " Mbps" << std::endl;

    Simulator::Destroy();
    return 0;
}