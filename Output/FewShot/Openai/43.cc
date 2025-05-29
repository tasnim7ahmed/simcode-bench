#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <iomanip>
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HiddenNodes80211nAggregation");

int main(int argc, char* argv[])
{
    bool enableRtsCts = false;
    uint32_t aggregatedMpduNum = 16;
    uint32_t payloadSize = 1472;
    double simulationTime = 10.0;
    double expectedThroughput = 10.0;

    CommandLine cmd;
    cmd.AddValue("rtsCts", "Enable/Disable RTS/CTS mechanism", enableRtsCts);
    cmd.AddValue("aggregateNum", "Number of aggregated MPDUs", aggregatedMpduNum);
    cmd.AddValue("payloadSize", "Payload size per UDP packet (bytes)", payloadSize);
    cmd.AddValue("simTime", "Simulation time (seconds)", simulationTime);
    cmd.AddValue("expectedThroughput", "Expected throughput in Mbps", expectedThroughput);
    cmd.Parse(argc, argv);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    Ptr<YansWifiChannel> chanPtr = channel.Create();

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(chanPtr);
    phy.Set("RxGain", DoubleValue(0));
    phy.Set("TxPowerStart", DoubleValue(16));
    phy.Set("TxPowerEnd", DoubleValue(16));
    phy.Set("ShortGuardEnabled", BooleanValue(true));
    phy.SetErrorRateModel("ns3::NistErrorRateModel");

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n_2_4GHZ);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
        "DataMode", StringValue("HtMcs7"),
        "ControlMode", StringValue("HtMcs0"));

    HtWifiMacHelper mac = HtWifiMacHelper::Default();

    Ssid ssid = Ssid("ns3-80211n");

    mac.SetType("ns3::StaWifiMac",
        "Ssid", SsidValue(ssid),
        "ActiveProbing", BooleanValue(false),
        "BE_MaxAmsduSize", UintegerValue(7935),
        "MpduAggregator", StringValue("ns3::MsduStandardAggregator"),
        "BE_MaxAmpduSize", UintegerValue(65535),
        "BE_MaxAmpduNum", UintegerValue(aggregatedMpduNum)
    );
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac",
        "Ssid", SsidValue(ssid),
        "BE_MaxAmsduSize", UintegerValue(7935),
        "MpduAggregator", StringValue("ns3::MsduStandardAggregator"),
        "BE_MaxAmpduSize", UintegerValue(65535),
        "BE_MaxAmpduNum", UintegerValue(aggregatedMpduNum)
    );
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    if (enableRtsCts) {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(0));
    } else {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(2347));
    }

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    // Place AP at (0, 0, 0)
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    // Place STA1 at (3, 0, 0) - in range of AP but not STA2
    positionAlloc->Add(Vector(3.0, 0.0, 0.0));
    // Place STA2 at (0, 4, 0) - also in range of AP, but greater than 5m from STA1
    positionAlloc->Add(Vector(0.0, 4.0, 0.0));

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNodes);

    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    // UDP server (sink) on AP
    uint16_t port = 4000;
    ApplicationContainer serverApps;
    UdpServerHelper server(port);
    serverApps = server.Install(wifiApNode.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime + 1));

    // UDP client on each sta - saturated traffic
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
    {
        UdpClientHelper client(apInterface.GetAddress(0), port);
        client.SetAttribute("MaxPackets", UintegerValue(0));
        client.SetAttribute("Interval", TimeValue(Time("0.0001s"))); // very tight
        client.SetAttribute("PacketSize", UintegerValue(payloadSize));
        clientApps.Add(client.Install(wifiStaNodes.Get(i)));
    }

    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime + 1));

    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.EnablePcapAll("wifi-80211n-hidden");

    AsciiTraceHelper ascii;
    phy.EnableAsciiAll(ascii.CreateFileStream("wifi-80211n-hidden.tr"));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 1.1));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    double totalRxBytes = 0;
    for (auto it = stats.begin(); it != stats.end(); ++it) {
        totalRxBytes += it->second.rxBytes;
    }
    double throughput = (totalRxBytes * 8.0) / (simulationTime * 1000000.0); // Mbps

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Simulation time: " << simulationTime << " s" << std::endl;
    std::cout << "UDP payload size: " << payloadSize << " bytes" << std::endl;
    std::cout << "Aggregated MPDU per A-MPDU: " << aggregatedMpduNum << std::endl;
    std::cout << "RTS/CTS: " << (enableRtsCts ? "Enabled" : "Disabled") << std::endl;
    std::cout << "Total received bytes: " << (uint64_t)totalRxBytes << std::endl;
    std::cout << "Throughput: " << throughput << " Mbps" << std::endl;
    std::cout << "Expected throughput: " << expectedThroughput << " Mbps" << std::endl;
    if (throughput >= expectedThroughput*0.98 && throughput <= expectedThroughput*1.02) {
        std::cout << "PASS: Throughput is within expected bounds." << std::endl;
    } else {
        std::cout << "FAIL: Throughput " << throughput << " Mbps is out of expected bounds [" 
                  << expectedThroughput*0.98 << ", " << expectedThroughput*1.02 << "] Mbps." << std::endl;
    }

    Simulator::Destroy();
    return 0;
}