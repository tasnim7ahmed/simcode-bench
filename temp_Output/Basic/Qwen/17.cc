#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiMultiNetworkAggregation");

int main(int argc, char *argv[]) {
    uint32_t payloadSize = 1472; // bytes
    double simulationTime = 10.0; // seconds
    double distance = 1.0; // meters
    bool enableRtsCts = false;

    CommandLine cmd;
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("distance", "Distance between station and AP (m)", distance);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS handshake", enableRtsCts);
    cmd.Parse(argc, argv);

    NodeContainer apNodes, staNodes;
    apNodes.Create(4);
    staNodes.Create(4);

    YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();
    phyHelper.SetChannel(channelHelper.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);

    if (enableRtsCts) {
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                     "DataMode", StringValue("HtMcs7"),
                                     "ControlMode", StringValue("HtMcs0"),
                                     "RtsCtsThreshold", UintegerValue(0));
    } else {
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                     "DataMode", StringValue("HtMcs7"),
                                     "ControlMode", StringValue("HtMcs0"));
    }

    Config::SetDefault("ns3::WifiMacQueueItem::MaxPacketQueueSize", QueueSizeValue(QueueSize("1000p")));

    NetDeviceContainer apDevices[4], staDevices[4];

    // Channel numbers: 36, 40, 44, 48
    uint8_t channels[4] = {36, 40, 44, 48};

    // Station A: default aggregation, A-MSDU disabled, A-MPDU enabled at 65KB
    wifi.SetMac("ns3::StaWifiMac",
                "QosSupported", BooleanValue(true),
                "AmsduSupport", BooleanValue(false),
                "AMPDUBufferSize", UintegerValue(65));
    phyHelper.Set("ChannelNumber", UintegerValue(channels[0]));
    staDevices[0] = wifi.Install(phyHelper, apDevices[0], staNodes.Get(0));

    // Station B: both A-MPDU and A-MSDU disabled
    wifi.SetMac("ns3::StaWifiMac",
                "QosSupported", BooleanValue(true),
                "AmsduSupport", BooleanValue(false),
                "AMPDUBufferSize", UintegerValue(0));
    phyHelper.Set("ChannelNumber", UintegerValue(channels[1]));
    staDevices[1] = wifi.Install(phyHelper, apDevices[1], staNodes.Get(1));

    // Station C: A-MSDU enabled with 8KB, A-MPDU disabled
    wifi.SetMac("ns3::StaWifiMac",
                "QosSupported", BooleanValue(true),
                "AmsduSupport", BooleanValue(true),
                "AmsduMaxSize", UintegerValue(8192),
                "AMPDUBufferSize", UintegerValue(0));
    phyHelper.Set("ChannelNumber", UintegerValue(channels[2]));
    staDevices[2] = wifi.Install(phyHelper, apDevices[2], staNodes.Get(2));

    // Station D: two-level aggregation, A-MPDU at 32KB and A-MSDU at 4KB
    wifi.SetMac("ns3::StaWifiMac",
                "QosSupported", BooleanValue(true),
                "AmsduSupport", BooleanValue(true),
                "AmsduMaxSize", UintegerValue(4096),
                "AMPDUBufferSize", UintegerValue(32));
    phyHelper.Set("ChannelNumber", UintegerValue(channels[3]));
    staDevices[3] = wifi.Install(phyHelper, apDevices[3], staNodes.Get(3));

    // Install APs
    Ssid ssid;
    for (uint8_t i = 0; i < 4; ++i) {
        ssid = Ssid("wifi-network-" + std::to_string(i));
        wifi.SetMac("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid),
                    "QosSupported", BooleanValue(true));
        apDevices[i] = wifi.Install(phyHelper, apNodes.Get(i));
    }

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(4),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);
    mobility.Install(staNodes);

    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[4];

    for (uint8_t i = 0; i < 4; ++i) {
        address.SetBase("10.0." + std::to_string(i) + ".0", "255.255.255.0");
        interfaces[i] = address.Assign(apDevices[i]);
        interfaces[i].Add(address.Assign(staDevices[i]));
    }

    // UDP Echo server setup on each AP
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps;
    for (uint8_t i = 0; i < 4; ++i) {
        serverApps = echoServer.Install(apNodes.Get(i));
        serverApps.Start(Seconds(0.0));
        serverApps.Stop(Seconds(simulationTime));
    }

    // UDP Echo client setup on each STA
    UdpEchoClientHelper echoClient(interfaces[0].GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.001)));
    echoClient.SetAttribute("PacketSize", UintegerValue(payloadSize));

    ApplicationContainer clientApps;
    for (uint8_t i = 0; i < 4; ++i) {
        echoClient.SetRemote(interfaces[i].GetAddress(0), 9);
        clientApps = echoClient.Install(staNodes.Get(i));
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(simulationTime));
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}