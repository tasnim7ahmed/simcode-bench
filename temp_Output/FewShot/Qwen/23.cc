#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiThroughputEvaluation");

class WifiThroughputExperiment {
public:
    WifiThroughputExperiment();
    void Run(uint8_t mcs, uint16_t channelWidth, bool shortGuardInterval, double frequency,
             bool enableUplinkOfdma, bool enableBsrp, uint32_t nStations, std::string trafficType,
             uint32_t payloadSize, uint32_t mpduBufferSize, bool enableAuxiliaryPhy,
             double simulationTime, double expectedMinThroughput, double expectedMaxThroughput);

private:
    void SetupNetwork(uint8_t mcs, uint16_t channelWidth, bool shortGuardInterval, double frequency,
                      bool enableUplinkOfdma, bool enableBsrp, uint32_t nStations,
                      bool enableAuxiliaryPhy);
    void SetupApplications(std::string trafficType, uint32_t payloadSize, double simulationTime);
    void CalculateThroughput(double simulationTime, double expectedMin, double expectedMax,
                              uint8_t mcs, uint16_t channelWidth, bool gi);
    void ReportResult(uint8_t mcs, uint16_t channelWidth, bool gi, double throughputMbps);

    NodeContainer wifiStaNodes;
    Ptr<Node> wifiApNode;
    NetDeviceContainer staDevices;
    NetDeviceContainer apDevice;
    Ipv4InterfaceContainer staInterfaces;
    Ipv4InterfaceContainer apInterface;
};

WifiThroughputExperiment::WifiThroughputExperiment() {
    wifiStaNodes.Create(0);
}

void WifiThroughputExperiment::Run(uint8_t mcs, uint16_t channelWidth, bool shortGuardInterval,
                                   double frequency, bool enableUplinkOfdma, bool enableBsrp,
                                   uint32_t nStations, std::string trafficType, uint32_t payloadSize,
                                   uint32_t mpduBufferSize, bool enableAuxiliaryPhy,
                                   double simulationTime, double expectedMinThroughput,
                                   double expectedMaxThroughput) {
    SetupNetwork(mcs, channelWidth, shortGuardInterval, frequency, enableUplinkOfdma, enableBsrp,
                 nStations, enableAuxiliaryPhy);
    SetupApplications(trafficType, payloadSize, simulationTime);
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Calculate total received bytes across all stations
    uint64_t totalRxBytes = 0;
    for (uint32_t i = 0; i < nStations; ++i) {
        Ptr<Ipv4> ipv4 = wifiStaNodes.Get(i)->GetObject<Ipv4>();
        if (!ipv4) continue;
        for (uint32_t j = 0; j < ipv4->GetNInterfaces(); ++j) {
            for (uint32_t k = 0; k < ipv4->GetNAddresses(j); ++k) {
                Ipv4Address addr = ipv4->GetAddress(j, k).GetLocal();
                if (addr.IsLocalhost()) continue;
                for (ApplicationContainer::Iterator it = ApplicationsStartStop::GetAllApps().Begin();
                     it != ApplicationsStartStop::GetAllApps().End(); ++it) {
                    if (DynamicCast<PacketSink>(*it)) {
                        totalRxBytes += DynamicCast<PacketSink>(*it)->GetTotalRx();
                    }
                }
            }
        }
    }

    double throughputMbps = (totalRxBytes * 8.0) / (simulationTime * 1e6);
    CalculateThroughput(simulationTime, expectedMinThroughput, expectedMaxThroughput, mcs,
                        channelWidth, shortGuardInterval);
    Simulator::Destroy();
}

void WifiThroughputExperiment::SetupNetwork(uint8_t mcs, uint16_t channelWidth,
                                           bool shortGuardInterval, double frequency,
                                           bool enableUplinkOfdma, bool enableBsrp,
                                           uint32_t nStations, bool enableAuxiliaryPhy) {
    wifiStaNodes = NodeContainer();
    wifiStaNodes.Create(nStations);
    wifiApNode = CreateObject<Node>();

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;

    // Set up Wi-Fi standard to IEEE 802.11be
    wifi.SetStandard(WIFI_STANDARD_80211be);

    // Configure Phy attributes
    phy.Set("ChannelWidth", UintegerValue(channelWidth));
    phy.Set("Frequency", DoubleValue(frequency));
    phy.Set("ShortGuardIntervalSupported", BooleanValue(shortGuardInterval));
    phy.Set("EnableUplinkOfdma", BooleanValue(enableUplinkOfdma));
    phy.Set("EnableBsrp", BooleanValue(enableBsrp));
    phy.Set("MpduBufferSize", UintegerValue(mpduBufferSize));
    phy.Set("EnableAuxiliaryPhy", BooleanValue(enableAuxiliaryPhy));

    // Set MCS fixed mode
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HeMcs" + std::to_string(mcs)),
                                 "ControlMode", StringValue("HeMcs" + std::to_string(mcs)));

    // Install AP
    mac.SetType("ns3::ApWifiMac");
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Install STAs
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("wifi-test")));
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    staInterfaces = address.Assign(staDevices);
    address.NewNetwork();
    apInterface = address.Assign(apDevice);
}

void WifiThroughputExperiment::SetupApplications(std::string trafficType, uint32_t payloadSize,
                                                double simulationTime) {
    uint16_t port = 9;

    if (trafficType == "tcp") {
        for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {
            Address sinkAddress(InetSocketAddress(staInterfaces.GetAddress(i), port));
            PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory",
                                              InetSocketAddress(Ipv4Address::GetAny(), port));
            ApplicationContainer sinkApp = packetSinkHelper.Install(wifiStaNodes.Get(i));
            sinkApp.Start(Seconds(0.0));
            sinkApp.Stop(Seconds(simulationTime));

            OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddress);
            onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
            onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
            onoff.SetAttribute("DataRate", DataRateValue(DataRate("1000Mbps")));
            onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));

            ApplicationContainer apps = onoff.Install(wifiApNode);
            apps.Start(Seconds(1.0));
            apps.Stop(Seconds(simulationTime));
        }
    } else if (trafficType == "udp") {
        for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {
            Address sinkAddress(InetSocketAddress(staInterfaces.GetAddress(i), port));
            PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory",
                                              InetSocketAddress(Ipv4Address::GetAny(), port));
            ApplicationContainer sinkApp = packetSinkHelper.Install(wifiStaNodes.Get(i));
            sinkApp.Start(Seconds(0.0));
            sinkApp.Stop(Seconds(simulationTime));

            OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
            onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
            onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
            onoff.SetAttribute("DataRate", DataRateValue(DataRate("1000Mbps")));
            onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));

            ApplicationContainer apps = onoff.Install(wifiApNode);
            apps.Start(Seconds(1.0));
            apps.Stop(Seconds(simulationTime));
        }
    }
}

void WifiThroughputExperiment::CalculateThroughput(double simulationTime, double expectedMin,
                                                  double expectedMax, uint8_t mcs,
                                                  uint16_t channelWidth, bool gi) {
    uint64_t totalRxBytes = 0;
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {
        for (ApplicationContainer::Iterator it = ApplicationsStartStop::GetAllApps().Begin();
             it != ApplicationsStartStop::GetAllApps().End(); ++it) {
            if (DynamicCast<PacketSink>(*it)) {
                totalRxBytes += DynamicCast<PacketSink>(*it)->GetTotalRx();
            }
        }
    }

    double throughputMbps = (totalRxBytes * 8.0) / (simulationTime * 1e6);
    double tolerance = 0.1; // 10% tolerance
    if (throughputMbps < expectedMin * (1 - tolerance) ||
        throughputMbps > expectedMax * (1 + tolerance)) {
        NS_FATAL_ERROR("Throughput out of bounds: " << throughputMbps << " Mbps for MCS "
                                                    << static_cast<uint32_t>(mcs)
                                                    << ", Channel Width " << channelWidth
                                                    << ", GI " << (gi ? "short" : "long"));
    }

    ReportResult(mcs, channelWidth, gi, throughputMbps);
}

void WifiThroughputExperiment::ReportResult(uint8_t mcs, uint16_t channelWidth, bool gi,
                                            double throughputMbps) {
    std::ostringstream oss;
    oss.precision(2);
    oss << std::fixed;
    oss << "MCS: " << static_cast<uint32_t>(mcs) << " | ";
    oss << "Channel Width: " << channelWidth << " MHz | ";
    oss << "GI: " << (gi ? "Short" : "Long") << " | ";
    oss << "Throughput: " << throughputMbps << " Mbps";
    NS_LOG_INFO(oss.str());
}

int main(int argc, char *argv[]) {
    uint8_t mcs = 11;
    uint16_t channelWidth = 160;
    bool shortGuardInterval = true;
    double frequency = 5.0e9;
    bool enableUplinkOfdma = true;
    bool enableBsrp = true;
    uint32_t nStations = 4;
    std::string trafficType = "tcp";
    uint32_t payloadSize = 1472;
    uint32_t mpduBufferSize = 1024;
    bool enableAuxiliaryPhy = false;
    double simulationTime = 10.0;
    double expectedMinThroughput = 500.0;
    double expectedMaxThroughput = 900.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("mcs", "Modulation and Coding Scheme (0-13)", mcs);
    cmd.AddValue("channelWidth", "Channel width in MHz (20, 40, 80, 160)", channelWidth);
    cmd.AddValue("shortGuardInterval", "Use short guard interval", shortGuardInterval);
    cmd.AddValue("frequency", "Operating frequency in GHz (2.4, 5, or 6)", frequency);
    cmd.AddValue("enableUplinkOfdma", "Enable uplink OFDMA", enableUplinkOfdma);
    cmd.AddValue("enableBsrp", "Enable BSRP", enableBsrp);
    cmd.AddValue("nStations", "Number of stations", nStations);
    cmd.AddValue("trafficType", "Traffic type (tcp or udp)", trafficType);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("mpduBufferSize", "MPDU buffer size", mpduBufferSize);
    cmd.AddValue("enableAuxiliaryPhy", "Enable auxiliary PHY", enableAuxiliaryPhy);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("expectedMinThroughput", "Minimum expected throughput in Mbps", expectedMinThroughput);
    cmd.AddValue("expectedMaxThroughput", "Maximum expected throughput in Mbps", expectedMaxThroughput);
    cmd.Parse(argc, argv);

    // Convert GHz to Hz
    frequency *= 1e9;

    // Ensure valid frequencies
    if (!(frequency == 2.4e9 || frequency == 5e9 || frequency == 6e9)) {
        NS_FATAL_ERROR("Invalid frequency: must be 2.4, 5, or 6 GHz");
    }

    LogComponentEnable("WifiThroughputEvaluation", LOG_LEVEL_INFO);

    WifiThroughputExperiment experiment;
    experiment.Run(mcs, channelWidth, shortGuardInterval, frequency, enableUplinkOfdma,
                   enableBsrp, nStations, trafficType, payloadSize, mpduBufferSize,
                   enableAuxiliaryPhy, simulationTime, expectedMinThroughput, expectedMaxThroughput);

    return 0;
}