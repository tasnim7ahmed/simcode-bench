#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/stats-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedWifiPerformance");

int main(int argc, char *argv[]) {
    bool verbose = false;
    uint32_t payloadSize = 1472;
    std::string dataRate = "54Mbps";
    std::string phyMode = "OfdmRate54Mbps";
    bool enableErpProtection = false;
    bool enableShortPreamble = false;
    bool enableShortSlotTime = false;
    std::string protocol = "Udp";

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if true", verbose);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("dataRate", "Data rate for OnOff application", dataRate);
    cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
    cmd.AddValue("erp", "Enable ERP protection", enableErpProtection);
    cmd.AddValue("shortPreamble", "Use short preamble", enableShortPreamble);
    cmd.AddValue("shortSlotTime", "Use short slot time", enableShortSlotTime);
    cmd.AddValue("protocol", "Transport protocol to use: Udp or Tcp", protocol);

    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiMac::Ssid", SsidValue(Ssid("ns-3-ssid")));

    NodeContainer staNodes;
    staNodes.Create(2);
    NodeContainer apNode;
    apNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

    WifiMacHelper mac;

    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(ssid),
                  "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "EnableBeaconJitter", BooleanValue(false));

    NetDeviceContainer apDevices = wifi.Install(phy, mac, apNode);

    if (enableErpProtection) {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/ErpProtection", BooleanValue(true));
    }

    if (enableShortPreamble) {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ShortPreambleEnabled", BooleanValue(true));
    }

    if (enableShortSlotTime) {
       Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Slot", TimeValue(MicroSeconds(9)));
    }

    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 5000;
    ApplicationContainer sinkApps;
    ApplicationContainer clientApps;

    if (protocol == "Udp") {
        UdpServerHelper echoServer(port);
        sinkApps = echoServer.Install(staNodes.Get(1));
        sinkApps.Start(Seconds(1.0));
        sinkApps.Stop(Seconds(10.0));

        UdpClientHelper echoClient(staInterfaces.GetAddress(1), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(4294967295));
        echoClient.SetAttribute("Interval", TimeValue(MicroSeconds(1)));
        echoClient.SetAttribute("PacketSize", UintegerValue(payloadSize));
        clientApps = echoClient.Install(staNodes.Get(0));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));
    } else if (protocol == "Tcp") {
        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        sinkApps = sink.Install(staNodes.Get(1));
        sinkApps.Start(Seconds(1.0));
        sinkApps.Stop(Seconds(10.0));

        BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(staInterfaces.GetAddress(1), port));
        source.SetAttribute("MaxBytes", UintegerValue(0));
        source.SetAttribute("SendSize", UintegerValue(payloadSize));
        clientApps = source.Install(staNodes.Get(0));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

    } else {
        std::cerr << "Invalid protocol specified: " << protocol << std::endl;
        return 1;
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(11.0));

    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
    }

    Simulator::Destroy();
    return 0;
}