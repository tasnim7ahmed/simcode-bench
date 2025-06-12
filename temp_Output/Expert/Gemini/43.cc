#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/command-line.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool enableRtsCts = false;
    uint32_t maxAmpduSize = 32;
    uint32_t payloadSize = 1472;
    double simulationTime = 10.0;
    double expectedThroughputMbps = 50.0;

    CommandLine cmd;
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS (default: false)", enableRtsCts);
    cmd.AddValue("maxAmpduSize", "Max AMPDU size (default: 32)", maxAmpduSize);
    cmd.AddValue("payloadSize", "Payload size (default: 1472)", payloadSize);
    cmd.AddValue("simulationTime", "Simulation time (default: 10.0)", simulationTime);
    cmd.AddValue("expectedThroughputMbps", "Expected throughput (default: 50.0)", expectedThroughputMbps);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue(enableRtsCts ? "0" : "2200"));

    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(2);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());
    phy.Set("TxPowerStart", DoubleValue(16.0206));
    phy.Set("TxPowerEnd", DoubleValue(16.0206));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n);
    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();

    Ssid ssid = Ssid("ns3-wifi");
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(2.0),
                                  "DeltaY", DoubleValue(2.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNodes);

    Ptr<ConstantPositionMobilityModel> apMobility = apNode.Get(0)->GetObject<ConstantPositionMobilityModel>();
    apMobility->SetPosition(Vector(2.0, 2.0, 0.0));
    Ptr<ConstantPositionMobilityModel> sta0Mobility = staNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
    sta0Mobility->SetPosition(Vector(0.0, 0.0, 0.0));
    Ptr<ConstantPositionMobilityModel> sta1Mobility = staNodes.Get(1)->GetObject<ConstantPositionMobilityModel>();
    sta1Mobility->SetPosition(Vector(4.0, 0.0, 0.0));

    InternetStackHelper internet;
    internet.Install(apNode);
    internet.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;
    UdpClientHelper client0(apInterface.GetAddress(0), port);
    client0.SetAttribute("MaxPackets", UintegerValue(0xffffffff));
    client0.SetAttribute("Interval", TimeValue(Seconds(0.00001)));
    client0.SetAttribute("PacketSize", UintegerValue(payloadSize));

    ApplicationContainer clientApps0 = client0.Install(staNodes.Get(0));
    clientApps0.Start(Seconds(2.0));
    clientApps0.Stop(Seconds(simulationTime));

    UdpClientHelper client1(apInterface.GetAddress(0), port);
    client1.SetAttribute("MaxPackets", UintegerValue(0xffffffff));
    client1.SetAttribute("Interval", TimeValue(Seconds(0.00001)));
    client1.SetAttribute("PacketSize", UintegerValue(payloadSize));

    ApplicationContainer clientApps1 = client1.Install(staNodes.Get(1));
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(simulationTime));


    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(apNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime + 1));

    phy.EnablePcap("wifi-hidden", apDevice.Get(0));
    phy.EnablePcap("wifi-hidden", staDevices);

    AsciiTraceHelper ascii;
    phy.EnableAsciiAll(ascii.CreateFileStream("wifi-hidden.tr"));

    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 1));

    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double totalThroughput = 0.0;
    for (auto i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if (t.destinationAddress == apInterface.GetAddress(0)) {
            totalThroughput += i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
        }
    }

    monitor->SerializeToXmlFile("wifi-hidden.flowmon", true, true);

    Simulator::Destroy();

    std::cout << "Total throughput: " << totalThroughput << " Mbps" << std::endl;

    if (totalThroughput < expectedThroughputMbps * 0.9) {
        std::cerr << "ERROR: Expected throughput not achieved." << std::endl;
        return 1;
    } else {
        std::cout << "Throughput verification passed." << std::endl;
    }

    return 0;
}