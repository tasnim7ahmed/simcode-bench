#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FourWifiNetworksAggregation");

static const uint32_t nNetworks = 4;

int main(int argc, char *argv[]) {
    double distance = 10.0;
    double simTime = 10.0;
    uint32_t payloadSize = 1472;
    bool rtsCts = false;

    CommandLine cmd;
    cmd.AddValue("distance", "Distance between station and AP", distance);
    cmd.AddValue("payloadSize", "Application payload size in bytes", payloadSize);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("rtsCts", "Enable/disable RTS/CTS", rtsCts);
    cmd.Parse(argc, argv);

    // Channels: 36, 40, 44, 48
    uint16_t channels[nNetworks] = {36, 40, 44, 48};
    std::string phyMode = "VhtMcs8"; // 802.11ac MCS 8

    NodeContainer staNodes[nNetworks], apNodes[nNetworks];
    NetDeviceContainer staDevices[nNetworks], apDevices[nNetworks];
    MobilityHelper mobility;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac);

    YansWifiPhyHelper phy[nNetworks];
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    WifiMacHelper mac;

    InternetStackHelper stack;
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer apIfs[nNetworks], staIfs[nNetworks];

    for (uint32_t i = 0; i < nNetworks; ++i) {
        staNodes[i].Create(1);
        apNodes[i].Create(1);

        // PHY config: one separate helper per network for unique channel
        phy[i] = YansWifiPhyHelper::Default();
        phy[i].SetChannel(channel.Create());
        phy[i].Set("ChannelWidth", UintegerValue(20));
        phy[i].Set("ShortGuardEnabled", BooleanValue(true));
        phy[i].Set("Frequency", UintegerValue(5180 + 5 * (channels[i] - 36))); // 5180MHz = channel 36
        phy[i].Set("ChannelNumber", UintegerValue(channels[i]));

        // MAC config
        Ssid ssid = Ssid("network" + std::to_string(i + 1));

        // Aggregation settings
        // Default: A-MSDU disabled, A-MPDU enabled (65535 bytes) [A]
        // B: Both disabled
        // C: A-MSDU enabled (8192 bytes), A-MPDU disabled
        // D: A-MPDU enabled (32768), A-MSDU enabled (4096)
        Mac48Address apMac = Mac48Address::Allocate();

        // Station
        mac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false),
                    "QosSupported", BooleanValue(true));

        staDevices[i] = wifi.Install(phy[i], mac, staNodes[i]);
        // AP
        mac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid),
                    "QosSupported", BooleanValue(true));
        apDevices[i] = wifi.Install(phy[i], mac, apNodes[i]);

        // Set aggregation
        Config::Set("/NodeList/" + std::to_string(staNodes[i].Get(0)->GetId()) +
                    "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/BE_EdcaTxopN/AmsduSupported",
                    BooleanValue(false));
        Config::Set("/NodeList/" + std::to_string(staNodes[i].Get(0)->GetId()) +
                    "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/BE_EdcaTxopN/AmpduSupported",
                    BooleanValue(false));
        if (i == 0) { // A: default, A-MSDU disabled, A-MPDU enabled 65KB
            Config::Set("/NodeList/" + std::to_string(staNodes[i].Get(0)->GetId()) +
                        "/DeviceList/0/$ns3::WifiNetDevice/Phy/MpduAggregator/MaxAmpduSize", UintegerValue(65535));
            Config::Set("/NodeList/" + std::to_string(staNodes[i].Get(0)->GetId()) +
                        "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/BE_EdcaTxopN/AmpduSupported", BooleanValue(true));
            Config::Set("/NodeList/" + std::to_string(staNodes[i].Get(0)->GetId()) +
                        "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/BE_EdcaTxopN/AmsduSupported", BooleanValue(false));
        } else if (i == 1) { // B: both disabled
            Config::Set("/NodeList/" + std::to_string(staNodes[i].Get(0)->GetId()) +
                        "/DeviceList/0/$ns3::WifiNetDevice/Phy/MpduAggregator/MaxAmpduSize", UintegerValue(0));
            Config::Set("/NodeList/" + std::to_string(staNodes[i].Get(0)->GetId()) +
                        "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/BE_EdcaTxopN/AmpduSupported", BooleanValue(false));
            Config::Set("/NodeList/" + std::to_string(staNodes[i].Get(0)->GetId()) +
                        "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/BE_EdcaTxopN/AmsduSupported", BooleanValue(false));
        } else if (i == 2) { // C: A-MSDU enabled 8KB, A-MPDU disabled
            Config::Set("/NodeList/" + std::to_string(staNodes[i].Get(0)->GetId()) +
                        "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/MaxAmsduSize", UintegerValue(8192));
            Config::Set("/NodeList/" + std::to_string(staNodes[i].Get(0)->GetId()) +
                        "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/BE_EdcaTxopN/AmsduSupported", BooleanValue(true));
            Config::Set("/NodeList/" + std::to_string(staNodes[i].Get(0)->GetId()) +
                        "/DeviceList/0/$ns3::WifiNetDevice/Phy/MpduAggregator/MaxAmpduSize", UintegerValue(0));
            Config::Set("/NodeList/" + std::to_string(staNodes[i].Get(0)->GetId()) +
                        "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/BE_EdcaTxopN/AmpduSupported", BooleanValue(false));
        } else if (i == 3) { // D: A-MPDU 32KB, A-MSDU 4KB
            Config::Set("/NodeList/" + std::to_string(staNodes[i].Get(0)->GetId()) +
                        "/DeviceList/0/$ns3::WifiNetDevice/Phy/MpduAggregator/MaxAmpduSize", UintegerValue(32768));
            Config::Set("/NodeList/" + std::to_string(staNodes[i].Get(0)->GetId()) +
                        "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/MaxAmsduSize", UintegerValue(4096));
            Config::Set("/NodeList/" + std::to_string(staNodes[i].Get(0)->GetId()) +
                        "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/BE_EdcaTxopN/AmpduSupported", BooleanValue(true));
            Config::Set("/NodeList/" + std::to_string(staNodes[i].Get(0)->GetId()) +
                        "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/BE_EdcaTxopN/AmsduSupported", BooleanValue(true));
        }

        // RTS/CTS
        if (rtsCts) {
            Config::Set("/NodeList/" + std::to_string(staNodes[i].Get(0)->GetId()) +
                        "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/RtsCtsThreshold", UintegerValue(0));
        } else {
            Config::Set("/NodeList/" + std::to_string(staNodes[i].Get(0)->GetId()) +
                        "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/RtsCtsThreshold", UintegerValue(999999));
        }

        stack.Install(staNodes[i]);
        stack.Install(apNodes[i]);
    }

    // Mobility (place AP and STA for each network at increasing X locations)
    for (uint32_t i = 0; i < nNetworks; ++i) {
        Ptr<ListPositionAllocator> staPos = CreateObject<ListPositionAllocator>();
        staPos->Add(Vector(distance * (2 * i), distance, 0.0));
        mobility.SetPositionAllocator(staPos);
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(staNodes[i]);
        Ptr<ListPositionAllocator> apPos = CreateObject<ListPositionAllocator>();
        apPos->Add(Vector(distance * (2 * i), 0.0, 0.0));
        mobility.SetPositionAllocator(apPos);
        mobility.Install(apNodes[i]);
    }

    // IP assignment
    for (uint32_t i = 0; i < nNetworks; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << (i+1) << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        apIfs[i] = address.Assign(apDevices[i]);
        staIfs[i] = address.Assign(staDevices[i]);
    }

    // Install traffic generators
    uint16_t port = 5000;
    ApplicationContainer apps;
    for (uint32_t i = 0; i < nNetworks; ++i) {
        // Sink on AP
        PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(apIfs[i].GetAddress(0), port));
        apps.Add(sink.Install(apNodes[i].Get(0)));
        // CBR client on STA
        OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(apIfs[i].GetAddress(0), port));
        onoff.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
        onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
        onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
        onoff.SetAttribute("StopTime", TimeValue(Seconds(simTime)));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        apps.Add(onoff.Install(staNodes[i].Get(0)));
    }

    Simulator::Stop(Seconds(simTime+1));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}