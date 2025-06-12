#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    uint32_t payloadSize = 1024;
    double simulationTime = 10.0;
    double distance = 1.0;
    bool enableRtsCts = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("distance", "Distance between station and AP in meters", distance);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS handshake", enableRtsCts);
    cmd.Parse(argc, argv);

    // Enable packet logging if needed
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    NodeContainer apNodes[4];
    NodeContainer staNodes[4];

    for (int i = 0; i < 4; ++i) {
        apNodes[i].Create(1);
        staNodes[i].Create(1);
    }

    YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();
    phyHelper.SetChannel(channelHelper.Create());

    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211ac);

    WifiMacHelper macHelper;
    Ssid ssid;

    NetDeviceContainer apDevices[4];
    NetDeviceContainer staDevices[4];

    Ipv4InterfaceContainer apInterfaces[4];
    Ipv4InterfaceContainer staInterfaces[4];

    for (int i = 0; i < 4; ++i) {
        // Set different channels: 36, 40, 44, 48
        uint8_t channelNumber = 36 + i * 4;
        phyHelper.Set("ChannelNumber", UintegerValue(channelNumber));

        // Configure aggregation settings per station
        if (i == 0) { // Station A: A-MPDU enabled at 65KB, A-MSDU disabled
            wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                               "DataMode", StringValue("VhtMcs9"),
                                               "ControlMode", StringValue("VhtMcs9"));
            Config::SetDefault("ns3::WifiMacQueueItem::MaxAmsduSize", UintegerValue(0));
            Config::SetDefault("ns3::WifiMacQueueItem::MaxAmpduSize", UintegerValue(65535));
        } else if (i == 1) { // Station B: Both A-MPDU and A-MSDU disabled
            wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                               "DataMode", StringValue("VhtMcs9"),
                                               "ControlMode", StringValue("VhtMcs9"));
            Config::SetDefault("ns3::WifiMacQueueItem::MaxAmsduSize", UintegerValue(0));
            Config::SetDefault("ns3::WifiMacQueueItem::MaxAmpduSize", UintegerValue(0));
        } else if (i == 2) { // Station C: A-MSDU enabled at 8KB, A-MPDU disabled
            wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                               "DataMode", StringValue("VhtMcs9"),
                                               "ControlMode", StringValue("VhtMcs9"));
            Config::SetDefault("ns3::WifiMacQueueItem::MaxAmsduSize", UintegerValue(8192));
            Config::SetDefault("ns3::WifiMacQueueItem::MaxAmpduSize", UintegerValue(0));
        } else if (i == 3) { // Station D: Two-level aggregation: A-MPDU 32KB, A-MSDU 4KB
            wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                               "DataMode", StringValue("VhtMcs9"),
                                               "ControlMode", StringValue("VhtMcs9"));
            Config::SetDefault("ns3::WifiMacQueueItem::MaxAmsduSize", UintegerValue(4096));
            Config::SetDefault("ns3::WifiMacQueueItem::MaxAmpduSize", UintegerValue(32768));
        }

        // Install access point
        ssid = Ssid("wifi-network-" + std::to_string(i));
        macHelper.SetType("ns3::ApWifiMac",
                          "Ssid", SsidValue(ssid),
                          "BeaconInterval", TimeValue(Seconds(2.5)),
                          "EnableBeaconJitter", BooleanValue(false));
        if (enableRtsCts) {
            macHelper.Set("RtsThreshold", UintegerValue(1));
        }
        apDevices[i] = wifiHelper.Install(phyHelper, macHelper, apNodes[i]);

        // Install station
        macHelper.SetType("ns3::StaWifiMac",
                          "Ssid", SsidValue(ssid),
                          "ActiveProbing", BooleanValue(false));
        staDevices[i] = wifiHelper.Install(phyHelper, macHelper, staNodes[i]);
    }

    // Mobility setup
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance * 2),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(4),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes[0]);
    mobility.Install(staNodes[0]);
    mobility.Install(apNodes[1]);
    mobility.Install(staNodes[1]);
    mobility.Install(apNodes[2]);
    mobility.Install(staNodes[2]);
    mobility.Install(apNodes[3]);
    mobility.Install(staNodes[3]);

    // Internet stack
    InternetStackHelper stack;
    for (int i = 0; i < 4; ++i) {
        stack.Install(apNodes[i]);
        stack.Install(staNodes[i]);
    }

    // IP assignment
    Ipv4AddressHelper address;
    for (int i = 0; i < 4; ++i) {
        std::string baseNetwork = "10." + std::to_string(i) + ".0.0";
        address.SetBase(baseNetwork.c_str(), "255.255.255.0");
        apInterfaces[i] = address.Assign(apDevices[i]);
        staInterfaces[i] = address.Assign(staDevices[i]);
    }

    // Applications
    uint16_t port = 9;
    for (int i = 0; i < 4; ++i) {
        // Server on AP
        UdpServerHelper server(port);
        ApplicationContainer serverApp = server.Install(apNodes[i]);
        serverApp.Start(Seconds(0.0));
        serverApp.Stop(Seconds(simulationTime));

        // Client on station
        UdpClientHelper client(apInterfaces[i].GetAddress(0), port);
        client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF)); // Infinite packets
        client.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
        client.SetAttribute("PacketSize", UintegerValue(payloadSize));

        ApplicationContainer clientApp = client.Install(staNodes[i]);
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(simulationTime));
    }

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}