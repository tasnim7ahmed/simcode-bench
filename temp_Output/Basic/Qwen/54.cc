#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/spectrum-wifi-phy.h"
#include "ns3/yans-wifi-channel.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wi-FiGoodputSimulation");

int main(int argc, char *argv[]) {
    uint32_t payloadSize = 1472;
    double simulationTime = 10.0;
    std::string trafficType = "udp";
    bool enableRtsCts = false;
    uint8_t mcsValue = 9;
    uint16_t channelWidth = 80;
    bool shortGuardInterval = true;
    double distance = 5.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("trafficType", "Transport protocol to use: tcp or udp", trafficType);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS handshake", enableRtsCts);
    cmd.AddValue("mcs", "MCS value (0-9)", mcsValue);
    cmd.AddValue("channelWidth", "Channel width in MHz (20, 40, 80, 160)", channelWidth);
    cmd.AddValue("shortGuardInterval", "Use short guard interval (true/false)", shortGuardInterval);
    cmd.AddValue("distance", "Distance between STA and AP (in meters)", distance);
    cmd.Parse(argc, argv);

    if (mcsValue > 9) {
        NS_ABORT_MSG("Invalid MCS value; must be between 0 and 9.");
    }

    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiPhyHelper phy;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;

    wifi.SetStandard(WIFI_STANDARD_80211ac);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("VhtMcs" + std::to_string(mcsValue)),
                                 "ControlMode", StringValue("VhtMcs" + std::to_string(mcsValue)));

    if (enableRtsCts) {
        mac.Set("RTSThreshold", UintegerValue(1));
    } else {
        mac.Set("RTSThreshold", UintegerValue(UINT_MAX));
    }

    Ssid ssid = Ssid("ns3-80211ac");
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
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNode);
    mobility.Install(wifiApNode);

    InternetStackHelper stack;
    stack.Install(wifiStaNode);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);

    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevices);

    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), 9));
    PacketSinkHelper packetSinkHelper((trafficType == "tcp") ? "ns3::TcpSocketFactory" : "ns3::UdpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApps = packetSinkHelper.Install(wifiStaNode.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime + 0.1));

    OnOffHelper onoff((trafficType == "tcp") ? "ns3::TcpSocketFactory" : "ns3::UdpSocketFactory",
                      Address(InetSocketAddress(staInterfaces.GetAddress(0), 9)));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
    onoff.SetAttribute("DataRate", StringValue("100Mbps"));

    ApplicationContainer sourceApps = onoff.Install(wifiApNode.Get(0));
    sourceApps.Start(Seconds(0.1));
    sourceApps.Stop(Seconds(simulationTime + 0.1));

    Simulator::Stop(Seconds(simulationTime + 0.1));
    Simulator::Run();

    uint64_t totalRxBytes = StaticCast<PacketSink>(sinkApps.Get(0))->GetTotalRx();
    double goodput = (totalRxBytes * 8.0) / (simulationTime * 1e6);

    std::cout << "Goodput: " << goodput << " Mbps | Traffic: " << trafficType
              << " | RTS/CTS: " << (enableRtsCts ? "ON" : "OFF")
              << " | MCS: " << (uint32_t)mcsValue
              << " | Channel Width: " << channelWidth << "MHz"
              << " | Guard Interval: " << (shortGuardInterval ? "Short" : "Long")
              << " | Distance: " << distance << "m" << std::endl;

    Simulator::Destroy();
    return 0;
}