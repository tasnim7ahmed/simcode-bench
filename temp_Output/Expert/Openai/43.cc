#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    bool enableRtsCts = false;
    uint32_t mpduAggregation = 32;
    uint32_t payloadSize = 1472;
    double simTime = 10.0;
    double expectedThroughput = 20.0; // Mbps
    double throughputTolerance = 1.0; // Mbps
    std::string dataRate = "300Mbps";

    CommandLine cmd;
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
    cmd.AddValue("mpduAggregation", "Number of aggregated MPDUs", mpduAggregation);
    cmd.AddValue("payloadSize", "UDP payload size (bytes)", payloadSize);
    cmd.AddValue("simTime", "Simulation time (s)", simTime);
    cmd.AddValue("expectedThroughput", "Expected AP MAC throughput (Mbps)", expectedThroughput);
    cmd.AddValue("throughputTolerance", "Throughput tolerance (Mbps)", throughputTolerance);
    cmd.AddValue("dataRate", "Wi-Fi PHY Data Rate", dataRate);
    cmd.Parse(argc, argv);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(5.0));

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n_2_4GHZ);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HtMcs7"),
                                 "ControlMode", StringValue("HtMcs0"));

    WifiMacHelper mac;
    Ssid ssid = Ssid("hidden-network");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    phy.EnablePcap("wifi-hidden-ap", apDevice.Get(0), true);
    phy.EnablePcap("wifi-hidden-sta1", staDevices.Get(0), true);
    phy.EnablePcap("wifi-hidden-sta2", staDevices.Get(1), true);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));   // AP at origin
    positionAlloc->Add(Vector(0.0, 5.0, 0.0));   // STA1 at max range along Y axis
    positionAlloc->Add(Vector(0.0, -5.0, 0.0));  // STA2 at max range along Y axis (opposite AP)
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNodes);

    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // Set MPDU Aggregation
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BE_MaxAmsduSize", UintegerValue(mpduAggregation * payloadSize));

    // Enable or disable RTS/CTS globally
    if (enableRtsCts) {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("0"));
    } else {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("999999"));
    }

    uint16_t port = 5000;
    ApplicationContainer serverApps;
    UdpServerHelper server(port);
    serverApps = server.Install(wifiApNode.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime + 1.0));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {
        UdpClientHelper client(apInterface.GetAddress(0), port);
        client.SetAttribute("MaxPackets", UintegerValue(0));
        client.SetAttribute("Interval", TimeValue(Time("0.00001s"))); // saturated
        client.SetAttribute("PacketSize", UintegerValue(payloadSize));
        clientApps.Add(client.Install(wifiStaNodes.Get(i)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simTime + 1.0));

    AsciiTraceHelper ascii;
    phy.EnableAsciiAll(ascii.CreateFileStream("wifi-hidden.tr"));

    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(simTime + 1.0));
    Simulator::Run();

    flowmon->CheckForLostPackets();
    double rxBytes = 0.0;
    double duration = 0.0;
    Ptr<Ipv4> ipv4 = wifiApNode.Get(0)->GetObject<Ipv4>();
    Ipv4Address apAddr = ipv4->GetAddress(1, 0).GetLocal();

    FlowMonitor::FlowStatsContainer stats = flowmon->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = flowmonHelper.GetClassifier()->FindFlow(iter->first);
        if (t.destinationAddress == apAddr && t.destinationPort == port) {
            rxBytes += iter->second.rxBytes;
            if (duration < (iter->second.timeLastRxPacket - iter->second.timeFirstTxPacket).GetSeconds())
                duration = (iter->second.timeLastRxPacket - iter->second.timeFirstTxPacket).GetSeconds();
        }
    }
    Simulator::Destroy();

    double throughput = (rxBytes * 8) / (duration * 1000000.0); // Mbps

    std::cout << "Simulation duration: " << duration << " s\n";
    std::cout << "Received bytes: " << rxBytes << '\n';
    std::cout << "Throughput: " << throughput << " Mbps\n";

    if (std::fabs(throughput - expectedThroughput) > throughputTolerance) {
        std::cout << "Throughput (" << throughput << " Mbps) out of expected bounds (" 
                  << expectedThroughput << " ± " << throughputTolerance << " Mbps)" << std::endl;
        return 1;
    } else {
        std::cout << "Throughput is within expected bounds." << std::endl;
    }

    return 0;
}