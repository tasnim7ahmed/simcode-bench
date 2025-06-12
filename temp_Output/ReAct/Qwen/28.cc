#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedWifiPerformanceTest");

int main(int argc, char *argv[]) {
    uint32_t payloadSizeUdp = 1000;
    uint32_t payloadSizeTcp = 1000;
    double simulationTime = 10.0;
    bool enableTcp = true;
    bool enableUdp = true;
    bool enableProtection = false;
    bool useShortPpdu = false;
    bool useShortSlot = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("payloadSizeUdp", "Payload size in bytes for UDP", payloadSizeUdp);
    cmd.AddValue("payloadSizeTcp", "Payload size in bytes for TCP", payloadSizeTcp);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("enableTcp", "Enable TCP flows", enableTcp);
    cmd.AddValue("enableUdp", "Enable UDP flows", enableUdp);
    cmd.AddValue("enableProtection", "Enable ERP protection mode", enableProtection);
    cmd.AddValue("useShortPpdu", "Use short PPDU format", useShortPpdu);
    cmd.AddValue("useShortSlot", "Use short slot time", useShortSlot);
    cmd.Parse(argc, argv);

    NodeContainer wifiStaNodesB, wifiStaNodesG, wifiStaNodesHT;
    wifiStaNodesB.Create(2);
    wifiStaNodesG.Create(2);
    wifiStaNodesHT.Create(2);
    NodeContainer apNodes;
    apNodes.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;

    // Configure HT (802.11n) station
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("mixed-network")));
    NetDeviceContainer staDevicesHT = wifi.Install(phy, mac, wifiStaNodesHT);

    // Configure g station
    wifi.SetStandard(WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("mixed-network")));
    NetDeviceContainer staDevicesG = wifi.Install(phy, mac, wifiStaNodesG);

    // Configure b station
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");
    NetDeviceContainer staDevicesB = wifi.Install(phy, mac, wifiStaNodesB);

    // Configure AP
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("mixed-network")),
                "BeaconInterval", TimeValue(Seconds(2.5)),
                "EnableBeaconJitter", BooleanValue(false),
                "RtsThreshold", UintegerValue(9000));
    if (useShortSlot) {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ShortSlotTimeEnabled", BooleanValue(true));
    }
    if (useShortPpdu) {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/ShortPpduSupported", BooleanValue(true));
    }
    if (enableProtection) {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/ErpProtectionMode", StringValue("NonErpCtsToSelf"));
    }

    NetDeviceContainer apDevice = wifi.Install(phy, mac, apNodes.Get(0));

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(wifiStaNodesB);
    mobility.Install(wifiStaNodesG);
    mobility.Install(wifiStaNodesHT);
    mobility.Install(apNodes);

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfacesB = address.Assign(staDevicesB);
    Ipv4InterfaceContainer staInterfacesG = address.Assign(staDevicesG);
    Ipv4InterfaceContainer staInterfacesHT = address.Assign(staDevicesHT);

    uint16_t port = 9;
    ApplicationContainer udpServerApps, tcpServerApps;

    if (enableUdp) {
        UdpServerHelper udpServer(port);
        udpServerApps.Add(udpServer.Install(apNodes.Get(0)));
        udpServerApps.Start(Seconds(0.0));
        udpServerApps.Stop(Seconds(simulationTime));

        UdpClientHelper udpClient(apInterface.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(4294967295U));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.001)));
        udpClient.SetAttribute("PacketSize", UintegerValue(payloadSizeUdp));

        for (uint32_t i = 0; i < wifiStaNodesB.GetN(); ++i) {
            ApplicationContainer clientApp = udpClient.Install(wifiStaNodesB.Get(i));
            clientApp.Start(Seconds(1.0));
            clientApp.Stop(Seconds(simulationTime));
        }
        for (uint32_t i = 0; i < wifiStaNodesG.GetN(); ++i) {
            ApplicationContainer clientApp = udpClient.Install(wifiStaNodesG.Get(i));
            clientApp.Start(Seconds(1.0));
            clientApp.Stop(Seconds(simulationTime));
        }
        for (uint32_t i = 0; i < wifiStaNodesHT.GetN(); ++i) {
            ApplicationContainer clientApp = udpClient.Install(wifiStaNodesHT.Get(i));
            clientApp.Start(Seconds(1.0));
            clientApp.Stop(Seconds(simulationTime));
        }
    }

    if (enableTcp) {
        PacketSinkHelper tcpSink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        tcpServerApps.Add(tcpSink.Install(apNodes.Get(0)));
        tcpServerApps.Start(Seconds(0.0));
        tcpServerApps.Stop(Seconds(simulationTime));

        OnOffHelper tcpOnOff("ns3::TcpSocketFactory", InetSocketAddress(apInterface.GetAddress(0), port));
        tcpOnOff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        tcpOnOff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        tcpOnOff.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
        tcpOnOff.SetAttribute("PacketSize", UintegerValue(payloadSizeTcp));
        tcpOnOff.SetAttribute("MaxBytes", UintegerValue(0));

        for (uint32_t i = 0; i < wifiStaNodesB.GetN(); ++i) {
            ApplicationContainer clientApp = tcpOnOff.Install(wifiStaNodesB.Get(i));
            clientApp.Start(Seconds(1.0));
            clientApp.Stop(Seconds(simulationTime));
        }
        for (uint32_t i = 0; i < wifiStaNodesG.GetN(); ++i) {
            ApplicationContainer clientApp = tcpOnOff.Install(wifiStaNodesG.Get(i));
            clientApp.Start(Seconds(1.0));
            clientApp.Stop(Seconds(simulationTime));
        }
        for (uint32_t i = 0; i < wifiStaNodesHT.GetN(); ++i) {
            ApplicationContainer clientApp = tcpOnOff.Install(wifiStaNodesHT.Get(i));
            clientApp.Start(Seconds(1.0));
            clientApp.Stop(Seconds(simulationTime));
        }
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();

    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = dynamic_cast<Ipv4FlowClassifier*>(monitor->GetClassifier())->FindFlow(iter->first);
        std::cout << "Flow ID: " << iter->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets = " << iter->second.txPackets << std::endl;
        std::cout << "  Rx Packets = " << iter->second.rxPackets << std::endl;
        std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / simulationTime / 1000 / 1000 << " Mbps" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}