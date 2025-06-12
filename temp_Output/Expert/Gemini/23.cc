#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config-store-module.h"
#include "ns3/command-line.h"
#include "ns3/log.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/boolean.h"
#include <iostream>
#include <fstream>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wifi7Throughput");

int main(int argc, char *argv[]) {
    bool verbose = false;
    std::string phyMode("HeSu20");
    uint32_t nWifi = 3;
    double simTime = 10;
    double distance = 50;
    std::string rate ("5Mbps");
    std::string trafficType = "tcp";
    uint32_t payloadSize = 1472;
    uint32_t mcs = 11;
    uint32_t channelWidth = 20;
    double guardInterval = 0.8;
    double minExpectedThroughput = 1.0;
    double maxExpectedThroughput = 1000.0;
    uint32_t mpduBufferSize = 8000;
    bool uplinkOfdma = false;
    bool bsrp = false;
    double frequency = 5000.0;
    bool puncIncluded = false;
    bool muMimoIncluded = false;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if set to true", verbose);
    cmd.AddValue("phyMode", "Wifi phy mode", phyMode);
    cmd.AddValue("nWifi", "Number of wifi STA devices", nWifi);
    cmd.AddValue("simTime", "Simulation run time in seconds", simTime);
    cmd.AddValue("distance", "Distance between access point and station devices", distance);
    cmd.AddValue("rate", "CBR traffic rate", rate);
    cmd.AddValue("trafficType", "Traffic type (tcp or udp)", trafficType);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("mcs", "MCS value", mcs);
    cmd.AddValue("channelWidth", "Channel width (MHz)", channelWidth);
    cmd.AddValue("guardInterval", "Guard interval (us)", guardInterval);
    cmd.AddValue("minExpectedThroughput", "Minimum expected throughput (Mbps)", minExpectedThroughput);
    cmd.AddValue("maxExpectedThroughput", "Maximum expected throughput (Mbps)", maxExpectedThroughput);
    cmd.AddValue("mpduBufferSize", "MPDU buffer size", mpduBufferSize);
    cmd.AddValue("uplinkOfdma", "Enable Uplink OFDMA", uplinkOfdma);
    cmd.AddValue("bsrp", "Enable BSRP", bsrp);
    cmd.AddValue("frequency", "Operating frequency (MHz)", frequency);
    cmd.AddValue("puncIncluded", "Puncturing included", puncIncluded);
    cmd.AddValue("muMimoIncluded", "MU-MIMO Included", muMimoIncluded);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("Wifi7Throughput", LOG_LEVEL_INFO);
        LogComponentEnable("TcpClient", LOG_LEVEL_INFO);
        LogComponentEnable("TcpServer", LOG_LEVEL_INFO);
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    Config::SetDefault("ns3::WifiMacQueue::MaxMPDUs", UintegerValue(mpduBufferSize));

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nWifi);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211be);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    NetDeviceContainer staDevices;
    NetDeviceContainer apDevices;

    // Configure AP
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconGeneration", BooleanValue(true),
                "BeaconInterval", TimeValue (Seconds (0.1)));

    apDevices = wifi.Install(phy, mac, wifiApNode);

    // Configure STA
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Mobility
    MobilityHelper mobility;

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(distance),
                                  "GridWidth", UintegerValue(nWifi),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Configure traffic
    uint16_t port = 50000;
    ApplicationContainer clientApps;
    ApplicationContainer serverApps;

    if (trafficType == "tcp") {
        TcpServerHelper server(port);
        serverApps = server.Install(wifiApNode.Get(0));
        serverApps.Start(Seconds(0.0));
        serverApps.Stop(Seconds(simTime + 1));

        for (uint32_t i = 0; i < nWifi; ++i) {
            TcpClientHelper client(apInterface.GetAddress(0), port);
            client.SetAttribute("Interval", TimeValue(Time(Seconds(0.01))));
            client.SetAttribute("MaxBytes", UintegerValue(0));
            client.SetAttribute("PacketSize", UintegerValue(payloadSize));
            clientApps.Add(client.Install(wifiStaNodes.Get(i)));
        }
    } else {
        UdpServerHelper server(port);
        serverApps = server.Install(wifiApNode.Get(0));
        serverApps.Start(Seconds(0.0));
        serverApps.Stop(Seconds(simTime + 1));

        for (uint32_t i = 0; i < nWifi; ++i) {
            UdpClientHelper client(apInterface.GetAddress(0), port);
            client.SetAttribute("Interval", TimeValue(Time(Seconds(0.01))));
            client.SetAttribute("MaxBytes", UintegerValue(0));
            client.SetAttribute("PacketSize", UintegerValue(payloadSize));
            clientApps.Add(client.Install(wifiStaNodes.Get(i)));
        }
    }

    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simTime));

    //Set the parameters

    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (channelWidth));
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/GuardInterval", DoubleValue (guardInterval));
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/Frequency", DoubleValue (frequency));
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_Qdisc/Txop/UplinkOfdmaEnable", BooleanValue (uplinkOfdma));
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_Qdisc/Txop/BsrpEnable", BooleanValue (bsrp));

    std::stringstream ss;
    ss << "HeSu";
    ss << channelWidth;
    phyMode = ss.str();
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PuncIncluded", BooleanValue (puncIncluded));
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/MuMimoIncluded", BooleanValue (muMimoIncluded));
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxSpatialStreams", UintegerValue(1));
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/HeSuPhy/HeMcs", UintegerValue(mcs));

    // Run simulation
    Simulator::Stop(Seconds(simTime + 1));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Run();

    // Calculate throughput
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalRxBytes = 0.0;
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if (t.destinationAddress == apInterface.GetAddress(0)) {
            totalRxBytes += i->second.rxBytes;
        }
    }

    double throughput = (totalRxBytes * 8) / (simTime * 1000000.0); // Mbps

    // Validate throughput
    if (throughput < minExpectedThroughput || throughput > maxExpectedThroughput) {
        std::cerr << "ERROR: Unexpected throughput value: " << throughput << " Mbps" << std::endl;
        Simulator::Destroy();
        return 1;
    }

    std::cout << "--------------------------------------------------------------------------------------" << std::endl;
    std::cout << " MCS | Channel Width | Guard Interval | Throughput (Mbps) " << std::endl;
    std::cout << "--------------------------------------------------------------------------------------" << std::endl;
    std::cout << std::setw(4) << mcs << " | "
              << std::setw(13) << channelWidth << " | "
              << std::setw(14) << guardInterval << " | "
              << std::setw(16) << throughput << std::endl;
    std::cout << "--------------------------------------------------------------------------------------" << std::endl;

    Simulator::Destroy();
    return 0;
}