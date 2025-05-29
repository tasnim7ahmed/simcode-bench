#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/command-line.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiHiddenStation");

int main(int argc, char *argv[]) {
    bool rtsCtsEnabled = false;
    uint32_t numAggregatedMPDU = 3;
    uint32_t payloadSize = 1472;
    double simulationTime = 10.0;
    double expectedThroughputLowerBound = 0.0;
    double expectedThroughputUpperBound = 100.0;
    std::string dataRate = "OfdmRate54Mbps";

    CommandLine cmd;
    cmd.AddValue("rtsCts", "Enable RTS/CTS (default: false)", rtsCtsEnabled);
    cmd.AddValue("numAggregatedMPDU", "Number of aggregated MPDUs (default: 3)", numAggregatedMPDU);
    cmd.AddValue("payloadSize", "Payload size in bytes (default: 1472)", payloadSize);
    cmd.AddValue("simulationTime", "Simulation time in seconds (default: 10.0)", simulationTime);
    cmd.AddValue("expectedThroughputLowerBound", "Expected throughput lower bound (default: 0.0)", expectedThroughputLowerBound);
    cmd.AddValue("expectedThroughputUpperBound", "Expected throughput upper bound (default: 100.0)", expectedThroughputUpperBound);
    cmd.AddValue("dataRate", "Data rate (default: OfdmRate54Mbps)", dataRate);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue("DsssRate1Mbps"));

    NodeContainer staNodes;
    staNodes.Create(2);
    NodeContainer apNode;
    apNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

    WifiMacHelper mac;

    Ssid ssid = Ssid("ns3-wifi");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconGeneration", BooleanValue(true),
                "BeaconInterval", TimeValue(Seconds(0.1)));

    NetDeviceContainer apDevices = wifi.Install(phy, mac, apNode);

    if (rtsCtsEnabled) {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("0"));
    } else {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));
    }

    Wifi80211nMacAggregationParameters aggParams;
    aggParams.maxAmsduSize = 7935;
    aggParams.maxAmpduSize = 65535;
    aggParams.maxNumAggregatedMpdu = numAggregatedMPDU;
    aggParams.mpduDensity = Wifi80211nMacAggregationParameters::A_MPDU_DENSITY_4_US;
    Wifi80211nHelper wifi80211n;
    wifi80211n.SetMPDUAggregationParameters(aggParams);

    MobilityHelper mobility;

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(staNodes);
    mobility.Install(apNode);

    // Hidden terminal setup
    Ptr<ConstantPositionMobilityModel> apMobility = apNode.Get(0)->GetObject<ConstantPositionMobilityModel>();
    apMobility->SetPosition(Vector(5.0, 5.0, 0.0));
    Ptr<ConstantPositionMobilityModel> sta1Mobility = staNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
    sta1Mobility->SetPosition(Vector(0.0, 0.0, 0.0));
    Ptr<ConstantPositionMobilityModel> sta2Mobility = staNodes.Get(1)->GetObject<ConstantPositionMobilityModel>();
    sta2Mobility->SetPosition(Vector(10.0, 10.0, 0.0));

    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    UdpClientHelper client1(apInterfaces.GetAddress(0), 50000);
    client1.SetAttribute("MaxPackets", UintegerValue(4294967295));
    client1.SetAttribute("Interval", TimeValue(Time("0.00002")));
    client1.SetAttribute("PacketSize", UintegerValue(payloadSize));
    ApplicationContainer clientApps1 = client1.Install(staNodes.Get(0));
    clientApps1.Start(Seconds(1.0));
    clientApps1.Stop(Seconds(simulationTime));

    UdpClientHelper client2(apInterfaces.GetAddress(0), 50001);
    client2.SetAttribute("MaxPackets", UintegerValue(4294967295));
    client2.SetAttribute("Interval", TimeValue(Time("0.00002")));
    client2.SetAttribute("PacketSize", UintegerValue(payloadSize));
    ApplicationContainer clientApps2 = client2.Install(staNodes.Get(1));
    clientApps2.Start(Seconds(1.0));
    clientApps2.Stop(Seconds(simulationTime));

    UdpServerHelper server1(50000);
    ApplicationContainer serverApps1 = server1.Install(apNode.Get(0));
    serverApps1.Start(Seconds(0.0));
    serverApps1.Stop(Seconds(simulationTime + 1));

    UdpServerHelper server2(50001);
    ApplicationContainer serverApps2 = server2.Install(apNode.Get(0));
    serverApps2.Start(Seconds(0.0));
    serverApps2.Stop(Seconds(simulationTime + 1));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 1));

    phy.EnablePcapAll("wifi-hiddenstation");

    AsciiTraceHelper ascii;
    phy.EnableAsciiAll(ascii.CreateFileStream("wifi-hiddenstation.tr"));

    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    double totalThroughput = 0.0;
    for (auto i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if (t.destinationAddress == apInterfaces.GetAddress(0)) {
            double throughput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
            totalThroughput += throughput;
            std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
            std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
            std::cout << "  Throughput: " << throughput << " Mbps\n";
        }
    }

    std::cout << "Total Throughput: " << totalThroughput << " Mbps\n";

    if (totalThroughput >= expectedThroughputLowerBound && totalThroughput <= expectedThroughputUpperBound) {
        std::cout << "Validation PASSED: Throughput is within the expected range.\n";
    } else {
        std::cout << "Validation FAILED: Throughput is outside the expected range.\n";
    }

    monitor->SerializeToXmlFile("wifi-hiddenstation.flowmon", true, true);

    Simulator::Destroy();

    return 0;
}