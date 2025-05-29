#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/command-line.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiQosSimulation");

int main(int argc, char *argv[]) {
    bool enablePcap = false;
    uint32_t payloadSize = 1472;
    double distance = 10.0;
    bool rtsCtsEnabled = false;
    double simulationTime = 10.0;

    CommandLine cmd;
    cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("distance", "Distance between AP and STA (meters)", distance);
    cmd.AddValue("rtsCts", "Enable RTS/CTS", rtsCtsEnabled);
    cmd.AddValue("simulationTime", "Simulation time (seconds)", simulationTime);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiMacQueue::MaxPackets", UintegerValue(100));

    YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();
    phyHelper.SetChannel(channelHelper.Create());

    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211n);

    NqosWifiMacHelper macHelper = NqosWifiMacHelper::Default();

    Ssid ssid1 = Ssid("ns3-wifi-qos-1");
    Ssid ssid2 = Ssid("ns3-wifi-qos-2");
    Ssid ssid3 = Ssid("ns3-wifi-qos-3");
    Ssid ssid4 = Ssid("ns3-wifi-qos-4");

    NodeContainer apNodes1, staNodes1, apNodes2, staNodes2, apNodes3, staNodes3, apNodes4, staNodes4;
    apNodes1.Create(1);
    staNodes1.Create(1);
    apNodes2.Create(1);
    staNodes2.Create(1);
    apNodes3.Create(1);
    staNodes3.Create(1);
    apNodes4.Create(1);
    staNodes4.Create(1);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(apNodes1);
    mobility.Install(staNodes1);
    mobility.Install(apNodes2);
    mobility.Install(staNodes2);
    mobility.Install(apNodes3);
    mobility.Install(staNodes3);
    mobility.Install(apNodes4);
    mobility.Install(staNodes4);

    macHelper.SetType("ns3::ApWifiMac",
                      "Ssid", SsidValue(ssid1));
    NetDeviceContainer apDevice1 = wifiHelper.Install(phyHelper, macHelper, apNodes1);

    macHelper.SetType("ns3::StaWifiMac",
                      "Ssid", SsidValue(ssid1),
                      "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice1 = wifiHelper.Install(phyHelper, macHelper, staNodes1);


    macHelper.SetType("ns3::ApWifiMac",
                      "Ssid", SsidValue(ssid2));
    NetDeviceContainer apDevice2 = wifiHelper.Install(phyHelper, macHelper, apNodes2);

    macHelper.SetType("ns3::StaWifiMac",
                      "Ssid", SsidValue(ssid2),
                      "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice2 = wifiHelper.Install(phyHelper, macHelper, staNodes2);

    macHelper.SetType("ns3::ApWifiMac",
                      "Ssid", SsidValue(ssid3));
    NetDeviceContainer apDevice3 = wifiHelper.Install(phyHelper, macHelper, apNodes3);

    macHelper.SetType("ns3::StaWifiMac",
                      "Ssid", SsidValue(ssid3),
                      "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice3 = wifiHelper.Install(phyHelper, macHelper, staNodes3);

    macHelper.SetType("ns3::ApWifiMac",
                      "Ssid", SsidValue(ssid4));
    NetDeviceContainer apDevice4 = wifiHelper.Install(phyHelper, macHelper, apNodes4);

    macHelper.SetType("ns3::StaWifiMac",
                      "Ssid", SsidValue(ssid4),
                      "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice4 = wifiHelper.Install(phyHelper, macHelper, staNodes4);

    if (rtsCtsEnabled) {
        Config::SetDefault("ns3::WifiMacQueue::RtsCtsThreshold", StringValue("0"));
    }

    InternetStackHelper stack;
    stack.Install(apNodes1);
    stack.Install(staNodes1);
    stack.Install(apNodes2);
    stack.Install(staNodes2);
    stack.Install(apNodes3);
    stack.Install(staNodes3);
    stack.Install(apNodes4);
    stack.Install(staNodes4);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface1 = address.Assign(apDevice1);
    Ipv4InterfaceContainer staInterface1 = address.Assign(staDevice1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface2 = address.Assign(apDevice2);
    Ipv4InterfaceContainer staInterface2 = address.Assign(staDevice2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface3 = address.Assign(apDevice3);
    Ipv4InterfaceContainer staInterface3 = address.Assign(staDevice3);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface4 = address.Assign(apDevice4);
    Ipv4InterfaceContainer staInterface4 = address.Assign(staDevice4);

    UdpServerHelper server1(9);
    ApplicationContainer serverApps1 = server1.Install(apNodes1.Get(0));
    serverApps1.Start(Seconds(1.0));
    serverApps1.Stop(Seconds(simulationTime));

    OnOffHelper client1("ns3::UdpSocketFactory", Address(InetSocketAddress(apInterface1.GetAddress(0), 9)));
    client1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    client1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    client1.SetAttribute("PacketSize", UintegerValue(payloadSize));
    client1.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    ApplicationContainer clientApps1 = client1.Install(staNodes1.Get(0));
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(simulationTime));

    UdpServerHelper server2(9);
    ApplicationContainer serverApps2 = server2.Install(apNodes2.Get(0));
    serverApps2.Start(Seconds(1.0));
    serverApps2.Stop(Seconds(simulationTime));

    OnOffHelper client2("ns3::UdpSocketFactory", Address(InetSocketAddress(apInterface2.GetAddress(0), 9)));
    client2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    client2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    client2.SetAttribute("PacketSize", UintegerValue(payloadSize));
    client2.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    ApplicationContainer clientApps2 = client2.Install(staNodes2.Get(0));
    clientApps2.Start(Seconds(2.0));
    clientApps2.Stop(Seconds(simulationTime));

    UdpServerHelper server3(9);
    ApplicationContainer serverApps3 = server3.Install(apNodes3.Get(0));
    serverApps3.Start(Seconds(1.0));
    serverApps3.Stop(Seconds(simulationTime));

    OnOffHelper client3("ns3::UdpSocketFactory", Address(InetSocketAddress(apInterface3.GetAddress(0), 9)));
    client3.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    client3.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    client3.SetAttribute("PacketSize", UintegerValue(payloadSize));
    client3.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    ApplicationContainer clientApps3 = client3.Install(staNodes3.Get(0));
    clientApps3.Start(Seconds(2.0));
    clientApps3.Stop(Seconds(simulationTime));

    UdpServerHelper server4(9);
    ApplicationContainer serverApps4 = server4.Install(apNodes4.Get(0));
    serverApps4.Start(Seconds(1.0));
    serverApps4.Stop(Seconds(simulationTime));

    OnOffHelper client4("ns3::UdpSocketFactory", Address(InetSocketAddress(apInterface4.GetAddress(0), 9)));
    client4.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    client4.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    client4.SetAttribute("PacketSize", UintegerValue(payloadSize));
    client4.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    ApplicationContainer clientApps4 = client4.Install(staNodes4.Get(0));
    clientApps4.Start(Seconds(2.0));
    clientApps4.Stop(Seconds(simulationTime));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    if (enablePcap) {
        phyHelper.EnablePcap("wifi-qos", apDevice1.Get(0));
        phyHelper.EnablePcap("wifi-qos", staDevice1.Get(0));
        phyHelper.EnablePcap("wifi-qos", apDevice2.Get(0));
        phyHelper.EnablePcap("wifi-qos", staDevice2.Get(0));
        phyHelper.EnablePcap("wifi-qos", apDevice3.Get(0));
        phyHelper.EnablePcap("wifi-qos", staDevice3.Get(0));
        phyHelper.EnablePcap("wifi-qos", apDevice4.Get(0));
        phyHelper.EnablePcap("wifi-qos", staDevice4.Get(0));
    }

    Simulator::Stop(Seconds(simulationTime));

    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (auto i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
    }

    Simulator::Destroy();
    return 0;
}