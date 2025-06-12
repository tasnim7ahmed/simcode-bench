#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("QosWi-FiSimulation");

class QosTxopTracer {
public:
    static void TraceTxop(Ptr<OutputStreamWrapper> stream, uint8_t tid, Time duration) {
        *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << (uint32_t)tid << "\t" << duration.GetMicroSeconds() << std::endl;
    }
};

int main(int argc, char *argv[]) {
    uint32_t payloadSize = 1472; // bytes
    double distance = 1.0; // meters
    bool enableRtsCts = false;
    double simulationTime = 10.0; // seconds
    bool pcapEnabled = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("distance", "Distance between nodes in meters", distance);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS for Wi-Fi", enableRtsCts);
    cmd.AddValue("simulationTime", "Duration of the simulation in seconds", simulationTime);
    cmd.AddValue("pcap", "Enable PCAP file generation", pcapEnabled);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::OnOffApplication::PacketSize", UintegerValue(payloadSize));
    Config::SetDefault("ns3::OnOffApplication::DataRate", StringValue("100Mbps"));

    NodeContainer apNodes[4];
    NodeContainer staNodes[4];

    for (int i = 0; i < 4; ++i) {
        apNodes[i].Create(1);
        staNodes[i].Create(1);
    }

    YansWifiPhyHelper phy;
    WifiChannelHelper channel = WifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    if (enableRtsCts) {
        Config::SetDefault("ns3::WifiMac::RtsThreshold", UintegerValue(1000));
    } else {
        Config::SetDefault("ns3::WifiMac::RtsThreshold", UintegerValue(UINT32_MAX));
    }

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211e);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("Htmixed-20"),
                                 "ControlMode", StringValue("Htmixed-20"));

    Ssid ssidBase = Ssid("qos-network");
    NetDeviceContainer apDevices[4], staDevices[4];

    InternetStackHelper stack;
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer apInterfaces[4], staInterfaces[4];

    OnOffHelper onOffHelper("ns3::UdpSocketFactory", Address());
    ApplicationContainer serverApps[4], clientApps[4];

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> txopStream = asciiTraceHelper.CreateFileStream("txop-trace.txt");

    for (int i = 0; i < 4; ++i) {
        ssidBase = Ssid("qos-network-" + std::to_string(i));

        WifiMacHelper mac;
        mac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssidBase),
                    "BeaconInterval", TimeValue(MicroSeconds(102400)),
                    "BeaconGeneration", BooleanValue(true));

        apDevices[i] = wifi.Install(phy, mac, apNodes[i]);

        mac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssidBase),
                    "ActiveProbing", BooleanValue(false));

        staDevices[i] = wifi.Install(phy, mac, staNodes[i]);

        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(i * 5.0),
                                      "DeltaX", DoubleValue(distance),
                                      "DeltaY", DoubleValue(0.0),
                                      "GridWidth", UintegerValue(2),
                                      "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(apNodes[i]);
        mobility.Install(staNodes[i]);

        stack.Install(apNodes[i]);
        stack.Install(staNodes[i]);

        address.SetBase("192.168." + std::to_string(i) + ".0", "255.255.255.0");
        apInterfaces[i] = address.Assign(apDevices[i]);
        staInterfaces[i] = address.Assign(staDevices[i]);

        PacketSinkHelper packetSink("ns3::UdpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), 9));
        serverApps[i] = packetSink.Install(apNodes[i]);
        serverApps[i].Start(Seconds(0.0));
        serverApps[i].Stop(Seconds(simulationTime));

        InetSocketAddress destAddr(apInterfaces[i].GetAddress(0), 9);
        onOffHelper.SetAttribute("Remote", AddressValue(destAddr));
        clientApps[i] = onOffHelper.Install(staNodes[i]);
        clientApps[i].Start(Seconds(1.0));
        clientApps[i].Stop(Seconds(simulationTime));

        if (pcapEnabled) {
            phy.EnablePcap("qos-wifi-" + std::to_string(i), apDevices[i]);
        }

        Config::ConnectWithoutContext("/NodeList/" + std::to_string(staNodes[i].Get(0)->GetId()) +
                                      "/DeviceList/0/Mac/Txop/Queue/Dequeue",
                                      MakeBoundCallback(&QosTxopTracer::TraceTxop, txopStream));
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalThroughput = 0.0;
    for (auto &[flowId, flowStats] : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowId);
        if (t.destinationPort == 9) {
            double throughput = (flowStats.rxBytes * 8.0) / (flowStats.timeLastRxPacket.GetSeconds() - flowStats.timeFirstTxPacket.GetSeconds()) / 1e6;
            NS_LOG_INFO("Flow " << flowId << " (" << t.sourceAddress << " -> " << t.destinationAddress << "): Throughput = " << throughput << " Mbps");
            totalThroughput += throughput;
        }
    }

    NS_LOG_INFO("Total Throughput: " << totalThroughput << " Mbps");

    Simulator::Destroy();
    return 0;
}