#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config-store-module.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiHiddenNodeAggregatedMpdu");

int main(int argc, char *argv[]) {
    bool rtsCtsEnabled = false;
    uint32_t numAggregatedMpdu = 10;
    uint32_t payloadSize = 1472;
    double simulationTime = 10.0;
    double expectedThroughput = 5.0; // Mbps

    CommandLine cmd;
    cmd.AddValue("rtsCts", "Enable RTS/CTS (1 for true, 0 for false)", rtsCtsEnabled);
    cmd.AddValue("numAggregatedMpdu", "Number of aggregated MPDUs", numAggregatedMpdu);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("expectedThroughput", "Expected throughput in Mbps", expectedThroughput);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiMacQueue::MaxSize", StringValue("500000 packets"));

    NodeContainer staNodes;
    staNodes.Create(2);
    NodeContainer apNode;
    apNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());
    phy.Set("Distance", DoubleValue(5));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
    Ssid ssid = Ssid("ns3-wifi");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconGeneration", BooleanValue(true));

    NetDeviceContainer apDevices = wifi.Install(phy, mac, apNode);

    if (rtsCtsEnabled) {
        Config::SetDefault("ns3::WifiMac::RtsCtsThreshold", StringValue("0"));
    }

    Config::Set("/NodeList/*/$ns3::WifiNetDevice/Mac/MpduAggregation/MaxAmsduSize", StringValue("7935"));
    Config::Set("/NodeList/*/$ns3::WifiNetDevice/Mac/MpduAggregation/MaxAmpduSize", StringValue("65535"));
    Config::Set("/NodeList/*/$ns3::WifiNetDevice/Mac/MpduAggregation/MaxNumAggregatedMpdu", UintegerValue(numAggregatedMpdu));

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNodes);
    mobility.Install(apNode);

    Ptr<ConstantPositionMobilityModel> apMobility = apNode.Get(0)->GetObject<ConstantPositionMobilityModel>();
    apMobility->SetPosition(Vector(10.0, 0.0, 0.0));

    InternetStackHelper internet;
    internet.Install(apNode);
    internet.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staNodeInterface;
    staNodeInterface = address.Assign(staDevices);
    Ipv4InterfaceContainer apNodeInterface;
    apNodeInterface = address.Assign(apDevices);

    uint16_t port = 9;

    UdpClientHelper client1(apNodeInterface.GetAddress(0), port);
    client1.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    client1.SetAttribute("Interval", TimeValue(Seconds(0.00001)));
    client1.SetAttribute("PacketSize", UintegerValue(payloadSize));

    UdpClientHelper client2(apNodeInterface.GetAddress(0), port);
    client2.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    client2.SetAttribute("Interval", TimeValue(Seconds(0.00001)));
    client2.SetAttribute("PacketSize", UintegerValue(payloadSize));

    ApplicationContainer clientApps1 = client1.Install(staNodes.Get(0));
    clientApps1.Start(Seconds(1.0));
    clientApps1.Stop(Seconds(simulationTime));

    ApplicationContainer clientApps2 = client2.Install(staNodes.Get(1));
    clientApps2.Start(Seconds(1.0));
    clientApps2.Stop(Seconds(simulationTime));


    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(apNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simulationTime + 1));

    phy.EnablePcap("wifi-hidden-node", apDevices.Get(0));

    AsciiTraceHelper ascii;
    phy.EnableAsciiAll(ascii.CreateFileStream("wifi-hidden-node.tr"));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 1));

    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double totalReceivedBytes = 0.0;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if (t.destinationAddress == apNodeInterface.GetAddress(0)) {
            totalReceivedBytes += i->second.rxBytes;
            std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
            std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
            std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";

        }
    }

    double actualThroughput = totalReceivedBytes * 8.0 / simulationTime / 1000000;
    std::cout << "Total Throughput: " << actualThroughput << " Mbps\n";

    if (actualThroughput < expectedThroughput * 0.9 || actualThroughput > expectedThroughput * 1.1) {
        std::cerr << "ERROR: Expected throughput not met!\n";
        return 1;
    }

    monitor->SerializeToXmlFile("wifi-hidden-node.flowmon", true, true);

    Simulator::Destroy();
    return 0;
}