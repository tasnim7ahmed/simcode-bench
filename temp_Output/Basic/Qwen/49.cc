#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiInterferenceSimulation");

class InterferenceHelper {
public:
    static Ptr<SpectrumValue> CreateSingleFrequencyWaveform(double powerW, double freq, double bandwidth) {
        Ptr<SpectrumModel> sm = SpectrumModelFactory().AddBand(CarrierFrequencyHz(freq).WithBandwidthAndCenterFrequency(bandwidth));
        Ptr<SpectrumValue> txPowerSpectrum = Create<SpectrumValue>(sm);
        (*txPowerSpectrum) = powerW / bandwidth;
        return txPowerSpectrum;
    }
};

int main(int argc, char *argv[]) {
    uint32_t simulationTime = 10;
    double distance = 5.0;
    uint8_t mcsIndex = 3;
    uint16_t channelWidth = 20;
    bool shortGuardInterval = false;
    double interfererPower = 0.1; // Watts
    std::string phyStandard = "WIFI_STANDARD_80211n";
    std::string errorModelType = "ns3::NistErrorRateModel";
    bool enablePcap = false;
    std::string trafficType = "tcp";

    CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Total simulation time (seconds)", simulationTime);
    cmd.AddValue("distance", "Distance between nodes (meters)", distance);
    cmd.AddValue("mcsIndex", "MCS index value", mcsIndex);
    cmd.AddValue("channelWidth", "Channel width in MHz", channelWidth);
    cmd.AddValue("shortGuardInterval", "Use short guard interval", shortGuardInterval);
    cmd.AddValue("interfererPower", "Interferer power in Watts", interfererPower);
    cmd.AddValue("phyStandard", "Wi-Fi standard (e.g., WIFI_STANDARD_80211n)", phyStandard);
    cmd.AddValue("errorModelType", "Error rate model type", errorModelType);
    cmd.AddValue("enablePcap", "Enable PCAP generation", enablePcap);
    cmd.AddValue("trafficType", "Traffic type: tcp or udp", trafficType);
    cmd.Parse(argc, argv);

    WifiStandard standard = WifiStandardNames().Find(phyStandard);
    if (standard == WIFI_STANDARD_UNSPECIFIED) {
        NS_FATAL_ERROR("Unknown Wi-Fi standard: " << phyStandard);
    }

    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiPhyHelper phy;
    phy.Set("TxPowerStart", DoubleValue(15.0));
    phy.Set("TxPowerEnd", DoubleValue(15.0));
    phy.Set("RxGain", DoubleValue(0.0));
    phy.Set("CcaEdThreshold", DoubleValue(-65.0));
    phy.SetErrorRateModel(errorModelType);

    WifiChannelHelper channel = WifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(standard);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue(WifiModeUtils::GetHeFallbackMode(mcsIndex, channelWidth, shortGuardInterval).GetUniqueName()),
                                 "ControlMode", StringValue(WifiMode("OfdmRate24Mbps")));

    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNode);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    NetDeviceContainer apDevices = wifi.Install(phy, mac, wifiApNode);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNode);
    mobility.Install(wifiApNode);

    InternetStackHelper stack;
    stack.Install(wifiStaNode);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);

    PacketSinkHelper sinkHelper(trafficType == "tcp" ? "ns3::TcpSocketFactory" : "ns3::UdpSocketFactory",
                                InetSocketAddress(Ipv4Address::GetAny(), 9));
    ApplicationContainer sinkApp = sinkHelper.Install(wifiStaNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simulationTime));

    OnOffHelper onoff(trafficType == "tcp" ? "ns3::TcpSocketFactory" : "ns3::UdpSocketFactory",
                      InetSocketAddress(staInterfaces.GetAddress(0), 9));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1472));

    ApplicationContainer srcApp = onoff.Install(wifiApNode.Get(0));
    srcApp.Start(Seconds(1.0));
    srcApp.Stop(Seconds(simulationTime));

    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback([](Ptr<const Packet> p, const Address &) {
        NS_LOG_UNCOND("Received packet of size " << p->GetSize());
    }));

    if (enablePcap) {
        phy.EnablePcap("wifi-interference-sim", apDevices.Get(0));
        phy.EnablePcap("wifi-interference-sim", staDevices.Get(0));
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    double rxPowerDbm = 10 * log10(phy.GetPhy(wifiStaNode.Get(0))->GetRxAntennaGain()) + 30;
    double noiseDbm = 10 * log10(phy.GetPhy(wifiStaNode.Get(0))->GetRxNoiseFigure()) + 30;
    double snr = rxPowerDbm - noiseDbm;

    NS_LOG_UNCOND("Transmission MCS Index: " << mcsIndex);
    NS_LOG_UNCOND("Channel Width (MHz): " << channelWidth);
    NS_LOG_UNCOND("Throughput: " << (sinkHelper.GetReceiveStats()->GetTotal() * 8.0) / (simulationTime * 1000000.0) << " Mbps");
    NS_LOG_UNCOND("Received Power: " << rxPowerDbm << " dBm");
    NS_LOG_UNCOND("Noise Floor: " << noiseDbm << " dBm");
    NS_LOG_UNCOND("SNR: " << snr << " dB");

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    Simulator::Destroy();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        NS_LOG_UNCOND("Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress);
        NS_LOG_UNCOND("  Tx Packets: " << i->second.txPackets);
        NS_LOG_UNCOND("  Rx Packets: " << i->second.rxPackets);
        NS_LOG_UNCOND("  Throughput: " << i->second.rxBytes * 8.0 / simulationTime / 1000000.0 << " Mbps");
    }

    return 0;
}