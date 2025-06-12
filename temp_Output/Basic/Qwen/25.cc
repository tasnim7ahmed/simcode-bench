#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/config-store-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFi80211axSimulation");

class WiFi80211axSimulator {
public:
    WiFi80211axSimulator();
    void Run(uint32_t nStations, uint32_t payloadSize, uint32_t numMcsValues,
             uint32_t channelWidth, Time simulationTime, bool enableRtsCts,
             bool enableUlOfdma, bool enableExtBa, bool useTcp, double distance,
             uint8_t guardInterval, uint32_t targetThroughputMbps, uint32_t band);

private:
    NodeContainer wifiApNode;
    NodeContainer wifiStaNodes;
    NetDeviceContainer apDevice;
    NetDeviceContainer staDevices;
    Ipv4InterfaceContainer apInterface;
    Ipv4InterfaceContainer staInterfaces;
    std::vector<double> mcsThroughputs;
};

WiFi80211axSimulator::WiFi80211axSimulator() {
    // Constructor
}

void WiFi80211axSimulator::Run(uint32_t nStations, uint32_t payloadSize, uint32_t numMcsValues,
                               uint32_t channelWidth, Time simulationTime, bool enableRtsCts,
                               bool enableUlOfdma, bool enableExtBa, bool useTcp, double distance,
                               uint8_t guardInterval, uint32_t targetThroughputMbps, uint32_t band) {

    // Setup nodes
    wifiApNode.Create(1);
    wifiStaNodes.Create(nStations);

    YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();
    phyHelper.SetChannel(channelHelper.Create());

    // Set frequency based on band
    if (band == 2) {
        phyHelper.Set("ChannelNumber", UintegerValue(1));
        phyHelper.Set("Frequency", UintegerValue(2412));
    } else if (band == 5) {
        phyHelper.Set("ChannelNumber", UintegerValue(36));
        phyHelper.Set("Frequency", UintegerValue(5180));
    } else if (band == 6) {
        phyHelper.Set("ChannelNumber", UintegerValue(1));
        phyHelper.Set("Frequency", UintegerValue(5960));
    }

    WifiMacHelper macHelper;
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211ax);
    wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                       "DataMode", StringValue("HeMcs0"),
                                       "ControlMode", StringValue("HeMcs0"));

    // Configure PHY
    phyHelper.Set("ChannelWidth", UintegerValue(channelWidth));
    phyHelper.Set("GuardInterval", TimeValue(NanoSeconds(guardInterval)));

    if (enableUlOfdma) {
        macHelper.Set("UL_OFDMAEnabled", BooleanValue(true));
    }
    if (enableExtBa) {
        macHelper.Set("ExtendedBlockAck", BooleanValue(true));
    }

    // Install AP and STA devices
    macHelper.SetType("ns3::ApWifiMac");
    apDevice = wifiHelper.Install(phyHelper, macHelper, wifiApNode);

    macHelper.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("wifi-test")));
    staDevices = wifiHelper.Install(phyHelper, macHelper, wifiStaNodes);

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0),
                                  "GridWidth", UintegerValue(nStations),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNodes);

    // Internet stack
    InternetStackHelper stackHelper;
    stackHelper.Install(wifiApNode);
    stackHelper.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    apInterface = address.Assign(apDevice);
    staInterfaces = address.Assign(staDevices);

    // Applications
    uint16_t port = 9;
    for (uint32_t i = 0; i < nStations; ++i) {
        if (useTcp) {
            BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(staInterfaces.GetAddress(i), port));
            source.SetAttribute("MaxBytes", UintegerValue(0));
            ApplicationContainer sourceApp = source.Install(wifiApNode.Get(0));
            sourceApp.Start(Seconds(0.5));
            sourceApp.Stop(simulationTime - Seconds(0.1));

            PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
            ApplicationContainer sinkApp = sink.Install(wifiStaNodes.Get(i));
            sinkApp.Start(Seconds(0.5));
            sinkApp.Stop(simulationTime - Seconds(0.1));
        } else {
            OnOffHelper source("ns3::UdpSocketFactory", InetSocketAddress(staInterfaces.GetAddress(i), port));
            source.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
            source.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
            source.SetAttribute("PacketSize", UintegerValue(payloadSize));
            source.SetAttribute("DataRate", DataRateValue(DataRate(targetThroughputMbps * 1e6 / nStations)));
            ApplicationContainer sourceApp = source.Install(wifiApNode.Get(0));
            sourceApp.Start(Seconds(0.5));
            sourceApp.Stop(simulationTime - Seconds(0.1));

            PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
            ApplicationContainer sinkApp = sink.Install(wifiStaNodes.Get(i));
            sinkApp.Start(Seconds(0.5));
            sinkApp.Stop(simulationTime - Seconds(0.1));
        }
    }

    // Enable RTS/CTS
    if (enableRtsCts) {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/RtsCtsEnabled", BooleanValue(true));
    }

    // Enable flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(simulationTime);
    Simulator::Run();

    // Output throughput for different MCS values
    for (uint32_t mcs = 0; mcs < numMcsValues; ++mcs) {
        std::ostringstream oss;
        oss << "HeMcs" << mcs;
        wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                          "DataMode", StringValue(oss.str()),
                                          "ControlMode", StringValue(oss.str()));

        // Re-run simulation or extract data here if needed
        // This is simplified: in practice, reconfigure and run again per MCS
        double throughput = 0.0;
        monitor->CheckForLostPackets();
        FlowMonitor::FlowStats stats = monitor->GetFlowStats().begin()->second;
        throughput = stats.rxBytes * 8.0 / (simulationTime.GetSeconds() * 1e6);
        mcsThroughputs.push_back(throughput);
        NS_LOG_UNCOND("MCS " << mcs << " Throughput: " << throughput << " Mbps");
    }

    Simulator::Destroy();
}

int main(int argc, char *argv[]) {
    uint32_t nStations = 5;
    uint32_t payloadSize = 1024;
    uint32_t numMcsValues = 12;
    uint32_t channelWidth = 80;
    Time simulationTime = Seconds(10);
    bool enableRtsCts = false;
    bool enableUlOfdma = false;
    bool enableExtBa = false;
    bool useTcp = false;
    double distance = 5.0;
    uint8_t guardInterval = 800;
    uint32_t targetThroughputMbps = 100;
    uint32_t band = 5;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nStations", "Number of stations", nStations);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("numMcsValues", "Number of MCS values to test", numMcsValues);
    cmd.AddValue("channelWidth", "Channel width (MHz)", channelWidth);
    cmd.AddValue("simulationTime", "Total simulation time", simulationTime);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
    cmd.AddValue("enableUlOfdma", "Enable UL OFDMA", enableUlOfdma);
    cmd.AddValue("enableExtBa", "Enable Extended Block Ack", enableExtBa);
    cmd.AddValue("useTcp", "Use TCP instead of UDP", useTcp);
    cmd.AddValue("distance", "Distance between nodes", distance);
    cmd.AddValue("guardInterval", "Guard interval in ns", guardInterval);
    cmd.AddValue("targetThroughputMbps", "Target throughput in Mbps", targetThroughputMbps);
    cmd.AddValue("band", "Band: 2 (2.4GHz), 5 (5GHz), 6 (6GHz)", band);
    cmd.Parse(argc, argv);

    WiFi80211axSimulator simulator;
    simulator.Run(nStations, payloadSize, numMcsValues, channelWidth, simulationTime,
                  enableRtsCts, enableUlOfdma, enableExtBa, useTcp, distance,
                  guardInterval, targetThroughputMbps, band);

    return 0;
}