#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessNetworkSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(3);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;

    wifi.SetStandard(WIFI_STANDARD_80211n_5MHz);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));

    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, wifiApNode);

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNodes);

    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevices);

    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);

    uint16_t port = 9;

    OnOffHelper onoff1("ns3::UdpSocketFactory", InetSocketAddress(staInterfaces.GetAddress(1), port));
    onoff1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff1.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff1.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer app1 = onoff1.Install(wifiStaNodes.Get(0));
    app1.Start(Seconds(1.0));
    app1.Stop(Seconds(10.0));

    OnOffHelper onoff2("ns3::UdpSocketFactory", InetSocketAddress(staInterfaces.Get(2), port));
    onoff2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff2.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff2.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer app2 = onoff2.Install(wifiStaNodes.Get(1));
    app2.Start(Seconds(1.0));
    app2.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp1 = sink.Install(wifiStaNodes.Get(1));
    sinkApp1.Start(Seconds(0.0));
    sinkApp1.Stop(Seconds(10.0));

    ApplicationContainer sinkApp2 = sink.Install(wifiStaNodes.Get(2));
    sinkApp2.Start(Seconds(0.0));
    sinkApp2.Stop(Seconds(10.0));

    AsciiTraceHelper asciiTraceHelper;
    phy.EnableAsciiAll(asciiTraceHelper.CreateFileStream("wireless-trace.tr"));
    phy.EnablePcapAll("wireless-pcap");

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalThroughput = 0;
    uint64_t totalPacketsDropped = 0;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if (t.destinationPort == 9) {
            NS_LOG_UNCOND("Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")");
            NS_LOG_UNCOND("  Tx Packets: " << i->second.txPackets);
            NS_LOG_UNCOND("  Rx Packets: " << i->second.rxPackets);
            NS_LOG_UNCOND("  Lost Packets: " << i->second.lostPackets);
            NS_LOG_UNCOND("  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000 << " Mbps");
            totalThroughput += i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1e6;
            totalPacketsDropped += i->second.lostPackets;
        }
    }

    NS_LOG_UNCOND("Total Throughput: " << totalThroughput << " Mbps");
    NS_LOG_UNCOND("Total Packet Loss: " << totalPacketsDropped << " packets");

    Simulator::Destroy();
    return 0;
}