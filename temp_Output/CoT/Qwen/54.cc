#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/spectrum-module.h"
#include <fstream>
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiGoodputSimulation");

int main(int argc, char *argv[]) {
    uint32_t payloadSize = 1472;           // bytes
    double simulationTime = 10;            // seconds
    double distance = 1.0;                 // meters
    std::string trafficType = "udp";       // "udp" or "tcp"
    bool enableRtsCts = false;             // RTS/CTS enabled?
    uint8_t mcsValue = 9;                  // MCS index (0-9)
    uint16_t channelWidth = 80;            // MHz (20, 40, 80, 160)
    bool shortGuardInterval = true;        // Guard interval

    CommandLine cmd;
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("distance", "Distance between station and AP", distance);
    cmd.AddValue("trafficType", "Traffic type: udp or tcp", trafficType);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS handshake", enableRtsCts);
    cmd.AddValue("mcsValue", "MCS value (0-9)", mcsValue);
    cmd.AddValue("channelWidth", "Channel width in MHz (20, 40, 80, 160)", channelWidth);
    cmd.AddValue("shortGuardInterval", "Use short guard interval", shortGuardInterval);
    cmd.Parse(argc, argv);

    if (mcsValue > 9) {
        NS_ABORT_MSG("MCS value must be between 0 and 9.");
    }

    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiPhyHelper phy;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("VhtMcs" + std::to_string(mcsValue)),
                                 "ControlMode", StringValue("VhtMcs0"));

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNode);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, wifiApNode);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNode);
    mobility.Install(wifiApNode);

    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevices);

    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelSettings",
                StringValue("{0, 0, " + std::to_string(channelWidth) + "}"));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ShortGuardIntervalSupported",
                BooleanValue(shortGuardInterval));
    if (enableRtsCts) {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/RTSThreshold", UintegerValue(1));
    } else {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/RTSThreshold", UintegerValue(3000));
    }

    uint16_t port = 9;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApps = packetSinkHelper.Install(wifiApNode.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime));

    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(apInterface.GetAddress(0), port));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));

    ApplicationContainer clientApps = onoff.Install(wifiStaNode.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    uint64_t rxBytes = StaticCast<PacketSink>(sinkApps.Get(0))->GetTotalRx();
    double throughput = static_cast<double>(rxBytes * 8) / (simulationTime * 1e6);

    std::cout << "Throughput: " << throughput << " Mbps | "
              << "Traffic: " << trafficType << ", "
              << "RTS/CTS: " << (enableRtsCts ? "Enabled" : "Disabled") << ", "
              << "MCS: " << (uint32_t)mcsValue << ", "
              << "Channel Width: " << channelWidth << " MHz, "
              << "Guard Interval: " << (shortGuardInterval ? "Short" : "Long")
              << std::endl;

    Simulator::Destroy();
    return 0;
}