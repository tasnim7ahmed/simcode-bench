#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiAggregationTest");

int main(int argc, char *argv[]) {
    uint32_t payloadSize = 1000;            // bytes
    double simulationTime = 10.0;           // seconds
    double distance = 1.0;                  // meters
    bool enableRtsCts = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("distance", "Distance between station and AP (m)", distance);
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
    wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                       "DataMode", StringValue("HtMcs7"),
                                       "ControlMode", StringValue("HtMcs0"));

    WifiMacHelper macHelper;
    Ssid ssid = Ssid("wifi-aggregation-test");

    NetDeviceContainer apDevices;
    NetDeviceContainer staDevices;

    std::vector<uint8_t> channels{36, 40, 44, 48};

    for (uint8_t i = 0; i < 4; ++i) {
        phyHelper.Set("ChannelNumber", UintegerValue(channels[i]));

        // Set up MAC for AP
        macHelper.SetType("ns3::ApWifiMac",
                          "Ssid", SsidValue(ssid),
                          "BeaconInterval", TimeValue(Seconds(2.5)));
        apDevices.Add(wifiHelper.Install(phyHelper, macHelper, apNodes.Get(i)));

        // Set up MAC for Station
        macHelper.SetType("ns3::StaWifiMac",
                          "Ssid", SsidValue(ssid),
                          "ActiveProbing", BooleanValue(false));

        if (enableRtsCts) {
            Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/RTSThreshold", UintegerValue(1));
            Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("ErpOfdmRate24Mbps"));
        } else {
            Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/RTSThreshold", UintegerValue(3000));
        }

        switch (i) {
            case 0: // Station A: A-MPDU enabled (65KB), A-MSDU disabled
                Config::Set("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/ShortGuardInterval", BooleanValue(true));
                Config::Set("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/Ampdu", BooleanValue(true));
                Config::Set("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/MaxAmpduSize", UintegerValue(65535));
                Config::Set("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/Amsdu", BooleanValue(false));
                break;
            case 1: // Station B: both A-MPDU and A-MSDU disabled
                Config::Set("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/Ampdu", BooleanValue(false));
                Config::Set("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/Amsdu", BooleanValue(false));
                break;
            case 2: // Station C: A-MSDU enabled (8KB), A-MPDU disabled
                Config::Set("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/Amsdu", BooleanValue(true));
                Config::Set("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/MaxAmsduSize", UintegerValue(8192));
                Config::Set("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/Ampdu", BooleanValue(false));
                break;
            case 3: // Station D: Two-level aggregation (A-MPDU 32KB, A-MSDU 4KB)
                Config::Set("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/Ampdu", BooleanValue(true));
                Config::Set("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/MaxAmpduSize", UintegerValue(32768));
                Config::Set("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/Amsdu", BooleanValue(true));
                Config::Set("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/MaxAmsduSize", UintegerValue(4096));
                break;
        }

        staDevices.Add(wifiHelper.Install(phyHelper, macHelper, staNodes.Get(i)));
    }

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
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

    for (uint8_t i = 0; i < 4; ++i) {
        NetDeviceContainer netDeviceContainer(apDevices.Get(i), staDevices.Get(i));
        Ipv4InterfaceContainer interfaces = address.Assign(netDeviceContainer);
        apInterfaces.Add(interfaces.Get(0));
        staInterfaces.Add(interfaces.Get(1));
        address.NewNetwork();
    }

    UdpServerHelper serverHelper;
    ApplicationContainer serverApps = serverHelper.Install(apNodes);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    UdpClientHelper clientHelper;
    clientHelper.SetAttribute("RemoteAddress", AddressValue(apInterfaces.GetAddress(0)));
    clientHelper.SetAttribute("RemotePort", UintegerValue(49153));
    clientHelper.SetAttribute("PacketSize", UintegerValue(payloadSize));
    clientHelper.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    clientHelper.SetAttribute("Interval", TimeValue(MicroSeconds(100)));

    ApplicationContainer clientApps;
    for (uint8_t i = 0; i < 4; ++i) {
        clientHelper.SetAttribute("RemoteAddress", AddressValue(apInterfaces.GetAddress(i)));
        clientApps = clientHelper.Install(staNodes.Get(i));
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(simulationTime - 0.1));
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}