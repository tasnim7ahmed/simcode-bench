#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiAggregationTest");

int main(int argc, char *argv[]) {
    uint32_t payloadSize = 1000; // bytes
    double simulationTime = 10.0; // seconds
    double distance = 1.0; // meters
    bool enableRtsCts = false;

    CommandLine cmd;
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("distance", "Distance between station and AP", distance);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS handshake", enableRtsCts);
    cmd.Parse(argc, argv);

    NodeContainer apNodes;
    NodeContainer staNodes;
    apNodes.Create(4);
    staNodes.Create(4);

    YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();
    phyHelper.SetChannel(channelHelper.Create());

    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211n_5GHZ);

    if (enableRtsCts) {
        wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                           "DataMode", StringValue("HtMcs7"),
                                           "ControlMode", StringValue("HtMcs0"),
                                           "RtsCtsThreshold", UintegerValue(0));
    } else {
        wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                           "DataMode", StringValue("HtMcs7"),
                                           "ControlMode", StringValue("HtMcs0"));
    }

    Config::SetDefault("ns3::WifiMacQueueItem::MaxPacketQueueSize", QueueSizeValue(QueueSize("1000p")));

    WifiMacHelper macHelper;
    Ssid ssid = Ssid("wifi-aggregation-test");

    NetDeviceContainer apDevices;
    NetDeviceContainer staDevices;

    for (uint32_t i = 0; i < 4; ++i) {
        std::ostringstream oss;
        oss << "channel-" << (36 + i * 4);
        phyHelper.Set("ChannelNumber", UintegerValue(36 + i * 4));
        phyHelper.Set("ChannelWidth", UintegerValue(20));

        macHelper.SetType("ns3::ApWifiMac",
                          "Ssid", SsidValue(ssid),
                          "BeaconInterval", TimeValue(Seconds(2.0)),
                          "BeaconJitter", UintegerValue(0));
        apDevices.Add(wifiHelper.Install(phyHelper, macHelper, apNodes.Get(i)));

        switch (i) {
            case 0: // A-MPDU enabled (65KB), A-MSDU disabled
                Config::Set("/NodeList/1/DeviceList/0/Mac/BE_Txop/MaxAmpduSize", UintegerValue(65535));
                Config::Set("/NodeList/1/DeviceList/0/Mac/BE_Txop/MaxAmsduSize", UintegerValue(0));
                break;
            case 1: // Both disabled
                Config::Set("/NodeList/1/DeviceList/0/Mac/BE_Txop/MaxAmpduSize", UintegerValue(0));
                Config::Set("/NodeList/1/DeviceList/0/Mac/BE_Txop/MaxAmsduSize", UintegerValue(0));
                break;
            case 2: // A-MSDU enabled (8KB), A-MPDU disabled
                Config::Set("/NodeList/1/DeviceList/0/Mac/BE_Txop/MaxAmpduSize", UintegerValue(0));
                Config::Set("/NodeList/1/DeviceList/0/Mac/BE_Txop/MaxAmsduSize", UintegerValue(8192));
                break;
            case 3: // Two-level aggregation: A-MPDU 32KB, A-MSDU 4KB
                Config::Set("/NodeList/1/DeviceList/0/Mac/BE_Txop/MaxAmpduSize", UintegerValue(32768));
                Config::Set("/NodeList/1/DeviceList/0/Mac/BE_Txop/MaxAmsduSize", UintegerValue(4096));
                break;
        }

        macHelper.SetType("ns3::StaWifiMac",
                          "Ssid", SsidValue(ssid),
                          "ActiveProbing", BooleanValue(false));
        staDevices.Add(wifiHelper.Install(phyHelper, macHelper, staNodes.Get(i)));
    }

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
    address.SetBase("10.0.0.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterfaces;
    Ipv4InterfaceContainer staInterfaces;

    for (uint32_t i = 0; i < 4; ++i) {
        NetDeviceContainer ndc;
        ndc.Add(apDevices.Get(i));
        ndc.Add(staDevices.Get(i));
        Ipv4InterfaceContainer interfaces = address.Assign(ndc);
        apInterfaces.Add(interfaces, 0);
        staInterfaces.Add(interfaces, 1);
        address.NewNetwork();
    }

    UdpServerHelper serverHelper(9);
    ApplicationContainer serverApps = serverHelper.Install(apNodes);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    UdpClientHelper clientHelper;
    clientHelper.SetAttribute("RemoteAddress", AddressAttributeValue(InetSocketAddress(apInterfaces.GetAddress(0), 9)));
    clientHelper.SetAttribute("RemotePort", UintegerValue(9));
    clientHelper.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    clientHelper.SetAttribute("PacketSize", UintegerValue(payloadSize));
    clientHelper.SetAttribute("Interval", TimeValue(MicroSeconds(100)));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < 4; ++i) {
        clientHelper.SetAttribute("RemoteAddress", AddressAttributeValue(InetSocketAddress(apInterfaces.GetAddress(i), 9)));
        ApplicationContainer app = clientHelper.Install(staNodes.Get(i));
        app.Start(Seconds(1.0));
        app.Stop(Seconds(simulationTime));
        clientApps.Add(app);
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}