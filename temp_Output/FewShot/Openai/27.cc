#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <iomanip>
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wifi11nMcsGoodput");

int main(int argc, char *argv[])
{
    double simTime = 10.0;
    std::string udpOrTcp = "UDP";
    std::string rtsCts = "off";
    double distance = 5.0;
    double expectedThroughput = 100.0; // Mbps
    uint32_t payloadSize = 1472; // bytes
    uint32_t mcs = 0, channelWidth = 20;
    bool shortGuard = false;
    std::string phyMode = "HtMcs0";

    CommandLine cmd(__FILE__);
    cmd.AddValue("simTime", "Simulation time (s)", simTime);
    cmd.AddValue("udpOrTcp", "Transport protocol (UDP|TCP)", udpOrTcp);
    cmd.AddValue("rtsCts", "Enable RTS/CTS (on|off)", rtsCts);
    cmd.AddValue("distance", "Distance between STA and AP (m)", distance);
    cmd.AddValue("expectedThroughput", "Expected throughput (Mbps)", expectedThroughput);
    cmd.AddValue("payloadSize", "Application payload size (bytes)", payloadSize);
    cmd.AddValue("mcs", "MCS index (0-7)", mcs);
    cmd.AddValue("channelWidth", "Channel width (20|40)", channelWidth);
    cmd.AddValue("shortGuard", "Short guard interval (0: long, 1: short)", shortGuard);
    cmd.Parse(argc, argv);

    if (udpOrTcp != "UDP" && udpOrTcp != "TCP")
        udpOrTcp = "UDP";

    std::ostringstream phyModeStream;
    phyModeStream << "HtMcs" << mcs;
    phyMode = phyModeStream.str();

    UintegerValue ctsThr = (rtsCts == "on") ? UintegerValue(0) : UintegerValue(2200);

    NodeContainer wifiStaNode, wifiApNode;
    wifiStaNode.Create(1);
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());
    phy.Set("ShortGuardEnabled", BooleanValue(shortGuard));
    phy.Set("ChannelWidth", UintegerValue(channelWidth));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue(phyMode),
                                 "ControlMode", StringValue("HtMcs0"),
                                 "RtsCtsThreshold", ctsThr);

    WifiMacHelper mac;
    Ssid ssid = Ssid("wifi-11n");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, wifiStaNode);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // AP
    positionAlloc->Add(Vector(distance, 0.0, 0.0)); // STA
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNode);

    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staIf, apIf;
    staIf = address.Assign(staDevice);
    apIf = address.Assign(apDevice);

    ApplicationContainer sinkApp, clientApp;
    uint16_t port = 9;

    if (udpOrTcp == "UDP") {
        UdpServerHelper udpServer(port);
        sinkApp = udpServer.Install(wifiApNode.Get(0));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(simTime + 1));

        UdpClientHelper udpClient(apIf.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(4294967295u));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.0)));
        udpClient.SetAttribute("PacketSize", UintegerValue(payloadSize));
        clientApp = udpClient.Install(wifiStaNode.Get(0));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(simTime));
    } else {
        Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
        sinkApp = packetSinkHelper.Install(wifiApNode.Get(0));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(simTime + 1));

        OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(apIf.GetAddress(0), port));
        onoff.SetAttribute("DataRate", DataRateValue(DataRate(static_cast<uint64_t>(expectedThroughput * 1e6))));
        onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
        onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
        onoff.SetAttribute("StopTime", TimeValue(Seconds(simTime)));
        clientApp = onoff.Install(wifiStaNode.Get(0));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simTime + 1));
    Simulator::Run();

    double rxBytes = 0, throughput = 0;
    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        if (t.destinationAddress == apIf.GetAddress(0) || t.sourceAddress == staIf.GetAddress(0)) {
            rxBytes = iter->second.rxBytes;
            throughput = rxBytes * 8.0 / (simTime * 1e6);
        }
    }

    std::cout << std::fixed << std::setprecision(4)
              << "MCS: " << mcs
              << ", ChannelWidth: " << channelWidth << " MHz"
              << ", ShortGI: " << (shortGuard ? "Yes" : "No")
              << ", Protocol: " << udpOrTcp
              << ", RTS/CTS: " << rtsCts
              << ", Distance: " << distance << " m"
              << ", Goodput: " << throughput << " Mbps"
              << std::endl;

    Simulator::Destroy();
    return 0;
}