#include <iostream>
#include <fstream>
#include <string>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiQosExample");

int main(int argc, char* argv[]) {
    // Enable logging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Simulation parameters
    uint32_t payloadSize = 1472; // bytes
    std::string dataRate = "5Mbps";
    std::string phyMode = "DsssRate11Mbps";
    double simulationTime = 10; // seconds
    double distance = 10.0; // meters
    bool verbose = false;
    bool tracing = false;
    bool enableRtsCts = false;

    // Command-line arguments
    CommandLine cmd;
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("dataRate", "Data rate for OnOff application", dataRate);
    cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("distance", "Distance between nodes", distance);
    cmd.AddValue("verbose", "Enable verbose logging", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
    cmd.Parse(argc, argv);

    // WifiHelper
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    // Wifi PHY channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());
    phy.Set("ShortGuardEnabled", BooleanValue(true));

    // Wifi MAC layer
    WifiMacHelper mac;
    Ssid ssid = Ssid("wifi-default");

    // Configure QoS parameters
    QosWifiMacHelper qosMac;

    // Network setup for each channel
    NodeContainer apNodes[4];
    NodeContainer staNodes[4];
    NetDeviceContainer apDevices[4];
    NetDeviceContainer staDevices[4];

    for (int i = 0; i < 4; ++i) {
        // Create nodes
        apNodes[i].Create(1);
        staNodes[i].Create(1);

        // Configure MAC layer
        mac.SetType("ns3::AdhocWifiMac",
                    "Ssid", SsidValue(ssid));

        qosMac.SetType("ns3::QosWifiMac",
                       "Ssid", SsidValue(ssid),
                       "BE_TxopLimit", TimeValue(MilliSeconds(enableRtsCts ? 0 : 100)), //Example settings for AC_BE, AC_VI
                       "VI_TxopLimit", TimeValue(MilliSeconds(enableRtsCts ? 0 : 50)));

        // Install Wifi devices
        apDevices[i] = wifi.Install(phy, mac, apNodes[i]);
        staDevices[i] = wifi.Install(phy, qosMac, staNodes[i]); //QosWifiMacHelper used here

        // Mobility models
        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(distance * i),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(0.0),
                                      "DeltaY", DoubleValue(distance));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(apNodes[i]);

        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(distance * i + distance),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(0.0),
                                      "DeltaY", DoubleValue(distance));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(staNodes[i]);
    }

    // Internet stack
    InternetStackHelper internet;
    for (int i = 0; i < 4; ++i) {
        internet.Install(apNodes[i]);
        internet.Install(staNodes[i]);
    }

    // Assign IP addresses
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer apInterfaces[4];
    Ipv4InterfaceContainer staInterfaces[4];

    for (int i = 0; i < 4; ++i) {
        address.SetBase("10.1." + std::to_string(i + 1) + ".0", "255.255.255.0");
        apInterfaces[i] = address.Assign(apDevices[i]);
        staInterfaces[i] = address.Assign(staDevices[i]);
    }

    // Applications (UDP)
    uint16_t port = 9; // Discard port
    OnOffHelper onoff("ns3::UdpClient", Address(InetSocketAddress(apInterfaces[0].GetAddress(0), port)));
    onoff.SetConstantRate(DataRate(dataRate));
    onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));

    ApplicationContainer clientApps[4];
    for (int i = 0; i < 4; ++i) {
        onoff.SetAttribute("Remote", AddressValue(InetSocketAddress(apInterfaces[i].GetAddress(0), port)));
        clientApps[i] = onoff.Install(staNodes[i].Get(0));
        clientApps[i].Start(Seconds(1.0));
        clientApps[i].Stop(Seconds(simulationTime + 1));
    }

    UdpServerHelper echoServer(port);
    ApplicationContainer serverApps[4];
    for (int i = 0; i < 4; ++i) {
        serverApps[i] = echoServer.Install(apNodes[i].Get(0));
        serverApps[i].Start(Seconds(0.0));
        serverApps[i].Stop(Seconds(simulationTime + 1));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Tracing
    if (tracing) {
        phy.EnablePcapAll("wifi-qos-example");
    }

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(simulationTime + 2));

    if (verbose) {
        LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    }

    Simulator::Run();

    // Print per flow statistics
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