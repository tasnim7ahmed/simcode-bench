#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiInterferenceSimulation");

class InterferenceHelper {
public:
    static Ptr<SpectrumValue> CreateInterferencePowerSpectralDensity(double txPowerW, uint16_t channelWidthMHz) {
        Ptr<SpectrumModel> sm = WifiSpectrumValueHelper::CreateWifiSpectrumModel(channelWidthMHz);
        Ptr<SpectrumValue> interferencePsd = Create<SpectrumValue>(sm);
        double txPowerDensity = txPowerW / channelWidthMHz / 1e6;
        for (SpectrumModel::ConstIterator it = sm->Begin(); it != sm->End(); ++it) {
            (*interferencePsd)[it->first] = txPowerDensity;
        }
        return interferencePsd;
    }
};

int main(int argc, char *argv[]) {
    // Default simulation parameters
    double simTime = 10.0;
    double distance = 5.0;
    uint8_t mcsIndex = 4;
    uint16_t channelWidth = 20;
    bool shortGuardInterval = false;
    double waveformPower = 0.01; // in watts
    std::string phyMode = "HtMcs4";
    std::string errorModelType = "ns3::NistErrorRateModel";
    bool enablePcap = false;
    bool enableUdp = true;
    bool enableTcp = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("simTime", "Total simulation time (seconds)", simTime);
    cmd.AddValue("distance", "Distance between nodes (meters)", distance);
    cmd.AddValue("mcsIndex", "Wi-Fi MCS index", mcsIndex);
    cmd.AddValue("channelWidth", "Channel width in MHz", channelWidth);
    cmd.AddValue("shortGuardInterval", "Use short guard interval", shortGuardInterval);
    cmd.AddValue("waveformPower", "Interferer power in watts", waveformPower);
    cmd.AddValue("phyMode", "Wi-Fi PHY mode (e.g., HtMcs4)", phyMode);
    cmd.AddValue("errorModelType", "Error model type", errorModelType);
    cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue(phyMode));
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(999999));
    Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", UintegerValue(999999));
    Config::SetDefault("ns3::WifiPhy::ChannelWidth", UintegerValue(channelWidth));
    Config::SetDefault("ns3::WifiPhy::ShortGuardIntervalSupported", BooleanValue(shortGuardInterval));
    Config::SetDefault("ns3::WifiPhy::ErrorRateModel", StringValue(errorModelType));

    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);

    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    NodeContainer interfererNode;
    interfererNode.Create(1);

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    Ptr<YansWifiChannel> channel = wifiChannel.Create();

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(mac, wifiStaNode);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(MicroSeconds(102400)));

    NetDeviceContainer apDevices;
    apDevices = wifi.Install(mac, wifiApNode);

    SpectrumChannelHelper spectrumChannelHelper = SpectrumChannelHelper::Default();
    spectrumChannelHelper.SetChannel("ns3::MultiModelSpectrumChannel");

    Ptr<SpectrumChannel> spectrumChannel = spectrumChannelHelper.Create();

    SpectrumInterferenceHelper interferenceHelper;
    interferenceHelper.SetTxPowerSpectralDensity(InterferenceHelper::CreateInterferencePowerSpectralDensity(waveformPower, channelWidth));
    interferenceHelper.Install(interfererNode.Get(0), spectrumChannel);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNode);
    mobility.Install(wifiApNode);
    mobility.Install(interfererNode);

    InternetStackHelper stack;
    stack.Install(wifiStaNode);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);

    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevices);

    uint16_t port = 9;

    if (enableUdp) {
        UdpServerHelper server(port);
        ApplicationContainer serverApps = server.Install(wifiStaNode.Get(0));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(simTime));

        UdpClientHelper client(staInterfaces.GetAddress(0), port);
        client.SetAttribute("MaxPackets", UintegerValue(4294967295u));
        client.SetAttribute("Interval", TimeValue(Seconds(0.001)));
        client.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApps = client.Install(wifiApNode.Get(0));
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(simTime));
    }

    if (enableTcp) {
        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApp = sink.Install(wifiStaNode.Get(0));
        sinkApp.Start(Seconds(1.0));
        sinkApp.Stop(Seconds(simTime));

        OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(staInterfaces.GetAddress(0), port));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onoff.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
        onoff.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = onoff.Install(wifiApNode.Get(0));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(simTime));
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    if (enablePcap) {
        wifiPhy.EnablePcap("wifi_interference_simulation", apDevices.Get(0));
        wifiPhy.EnablePcap("wifi_interference_simulation", staDevices.Get(0));
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / simTime / 1000 / 1000 << " Mbps\n";
        std::cout << "  Mean Delay: " << i->second.delaySum.GetSeconds() / i->second.rxPackets << " s\n";
        std::cout << "  Mean Jitter: " << i->second.jitterSum.GetSeconds() / (i->second.rxPackets > 1 ? i->second.rxPackets - 1 : 1) << " s\n";
    }

    Simulator::Destroy();
    return 0;
}