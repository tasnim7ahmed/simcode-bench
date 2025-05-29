#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/command-line.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/tcp-client-server-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/stats-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiInterference");

int main(int argc, char *argv[]) {
    bool verbose = false;
    uint32_t nWifi = 2;
    double simulationTime = 10;
    double distance = 10;
    uint32_t mcsIndex = 7;
    uint32_t channelWidthIndex = 0;
    uint32_t guardIntervalIndex = 0;
    double waveformPower = 0;
    std::string wifiType = "ns3::YansWifiPhy";
    bool pcapTracing = false;
    std::string errorModelType = "None";
    double errorRate = 0.0;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if true.", verbose);
    cmd.AddValue("nW", "Number of wifi STA/AP nodes", nWifi);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("distance", "Distance between nodes", distance);
    cmd.AddValue("mcsIndex", "MCS index (0-7)", mcsIndex);
    cmd.AddValue("channelWidthIndex", "Channel width index (0-3)", channelWidthIndex);
    cmd.AddValue("guardIntervalIndex", "Guard interval index (0-1)", guardIntervalIndex);
    cmd.AddValue("waveformPower", "Waveform power for non-wifi interferer", waveformPower);
    cmd.AddValue("wifiType", "Type of wifi phy (YansWifiPhy or SpectrumWifiPhy)", wifiType);
    cmd.AddValue("pcap", "Enable PCAP tracing", pcapTracing);
    cmd.AddValue("errorModel", "Type of error model (RateErrorModel or None)", errorModelType);
    cmd.AddValue("errorRate", "Error rate for RateErrorModel", errorRate);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("WifiInterference", LOG_LEVEL_INFO);
    }

    NodeContainer wifiNodes;
    wifiNodes.Create(nWifi);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiNodes.Get(0));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconGeneration", BooleanValue(true),
                "BeaconInterval", TimeValue(Seconds(0.1)));

    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, wifiNodes.Get(1));

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(nWifi),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    InternetStackHelper stack;
    stack.Install(wifiNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(apDevices);
    interfaces.Add(address.Assign(staDevices).Get(0));

    UdpClientServerHelper udp(interfaces.GetAddress(1), 9);
    udp.SetAttribute("MaxPackets", UintegerValue(1000000));
    udp.SetAttribute("Interval", TimeValue(MicroSeconds(10)));
    udp.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = udp.Install(wifiNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime));

    ApplicationContainer serverApps = udp.Install(wifiNodes.Get(1));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime + 1));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    if (pcapTracing) {
        phy.EnablePcap("wifi-interference", apDevices);
    }

    Simulator::Stop(Seconds(simulationTime + 1));
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