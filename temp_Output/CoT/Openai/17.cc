#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FourIndependentWifiNets");

int
main(int argc, char *argv[])
{
    uint32_t payloadSize = 1472;
    double simTime = 10.0;
    double distance = 10.0;
    bool enableRtsCts = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("payloadSize", "UDP payload size (bytes)", payloadSize);
    cmd.AddValue("simTime", "Simulation time (seconds)", simTime);
    cmd.AddValue("distance", "Distance between station and AP (meters)", distance);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS (true/false)", enableRtsCts);
    cmd.Parse(argc, argv);

    std::vector<uint16_t> channels = {36, 40, 44, 48};
    std::vector<Ssid> ssids = {Ssid("ssid-A"), Ssid("ssid-B"), Ssid("ssid-C"), Ssid("ssid-D")};

    NodeContainer aps, stations;
    aps.Create(4);
    stations.Create(4);

    YansWifiChannelHelper channelHelpers[4];
    Ptr<YansWifiChannel> wifiChannels[4];
    YansWifiPhyHelper phyHelpers[4];

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211ac);

    NetDeviceContainer apDevs[4], staDevs[4];

    // Aggregation settings for each station
    struct AggSettings {
        bool amsduEnabled;
        uint32_t amsduSize;
        bool ampduEnabled;
        uint32_t ampduSize;
    } agg[4];

    // A: default aggregation, A-MSDU off, A-MPDU on at 65KB
    agg[0] = {false, 0, true, 65 * 1024};

    // B: A-MSDU off, A-MPDU off
    agg[1] = {false, 0, false, 0};

    // C: A-MSDU on at 8KB, A-MPDU off
    agg[2] = {true, 8 * 1024, false, 0};

    // D: both on, A-MSDU 4KB, A-MPDU 32KB
    agg[3] = {true, 4 * 1024, true, 32 * 1024};

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < 4; ++i) {
        posAlloc->Add(Vector(i * distance * 3, 0.0, 0.0)); // APs separation
        posAlloc->Add(Vector(i * distance * 3 + distance, 0.0, 0.0)); // STAs nearby
    }
    mobility.SetPositionAllocator(posAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(aps);
    mobility.Install(stations);

    for (uint32_t i = 0; i < 4; ++i) {
        channelHelpers[i] = YansWifiChannelHelper::Default();
        wifiChannels[i] = channelHelpers[i].Create();
        phyHelpers[i] = YansWifiPhyHelper::Default();
        phyHelpers[i].SetChannel(wifiChannels[i]);
        phyHelpers[i].Set("ChannelNumber", UintegerValue(channels[i]));
        phyHelpers[i].Set("ChannelWidth", UintegerValue(20));
        phyHelpers[i].Set("Frequency", UintegerValue(5180 + 5 * (channels[i] - 36)));
        phyHelpers[i].Set("TxPowerStart", DoubleValue(16.0));
        phyHelpers[i].Set("TxPowerEnd", DoubleValue(16.0));

        // Configure aggregation for each station device
        WifiMacHelper macSta, macAp;

        // Station
        macSta.SetType("ns3::StaWifiMac",
                       "Ssid", SsidValue(ssids[i]),
                       "ActiveProbing", BooleanValue(false));
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                    "DataMode", StringValue("VhtMcs8"),
                                    "ControlMode", StringValue("VhtMcs8"));

        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(enableRtsCts ? 0 : 9999));
        Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", UintegerValue(4096));

        // Aggregation controls - per device in WifiRemoteStationManager/Phy on *specific path*
        std::ostringstream staPath, apPath;
        staPath << "/NodeList/" << (aps.GetN() + i) << "/DeviceList/0/$ns3::WifiNetDevice/Mac/";
        apPath  << "/NodeList/" << i << "/DeviceList/0/$ns3::WifiNetDevice/Mac/";

        staDevs[i] = wifi.Install(phyHelpers[i], macSta, stations.Get(i));
        macAp.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssids[i]));
        apDevs[i] = wifi.Install(phyHelpers[i], macAp, aps.Get(i));

        std::string staAggrPath = "/NodeList/" + std::to_string(aps.GetN() + i) +
                                  "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/*";
        std::string apAggrPath = "/NodeList/" + std::to_string(i) +
                                 "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::ApWifiMac/*";

        // A-MPDU
        Config::Set(staAggrPath + "BE_MaxAmpduSize", UintegerValue(agg[i].ampduEnabled ? agg[i].ampduSize : 0));
        Config::Set(apAggrPath  + "BE_MaxAmpduSize", UintegerValue(agg[i].ampduEnabled ? agg[i].ampduSize : 0));
        Config::Set(staAggrPath + "BE_EnableAmpdu", BooleanValue(agg[i].ampduEnabled));
        Config::Set(apAggrPath  + "BE_EnableAmpdu", BooleanValue(agg[i].ampduEnabled));

        // A-MSDU
        Config::Set(staAggrPath + "BE_MaxAmsduSize", UintegerValue(agg[i].amsduEnabled ? agg[i].amsduSize : 0));
        Config::Set(apAggrPath  + "BE_MaxAmsduSize", UintegerValue(agg[i].amsduEnabled ? agg[i].amsduSize : 0));
        Config::Set(staAggrPath + "BE_EnableAmsdu", BooleanValue(agg[i].amsduEnabled));
        Config::Set(apAggrPath  + "BE_EnableAmsdu", BooleanValue(agg[i].amsduEnabled));
    }

    InternetStackHelper stack;
    stack.Install(aps);
    stack.Install(stations);

    Ipv4AddressHelper ipv4;
    std::vector<Ipv4InterfaceContainer> apIfaces(4), staIfaces(4);
    for (uint32_t i = 0; i < 4; ++i) {
        std::ostringstream net;
        net << "10." << i+1 << ".0.0";
        ipv4.SetBase(net.str().c_str(), "255.255.255.0");
        apIfaces[i] = ipv4.Assign(apDevs[i]);
        staIfaces[i] = ipv4.Assign(staDevs[i]);
    }

    // Applications: UDP traffic from station to AP
    for (uint32_t i = 0; i < 4; ++i) {
        uint16_t port = 9000 + i;

        UdpServerHelper server(port);
        ApplicationContainer servApp = server.Install(aps.Get(i));
        servApp.Start(Seconds(0.0));
        servApp.Stop(Seconds(simTime + 1.0));

        UdpClientHelper client(apIfaces[i].GetAddress(0), port);
        client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
        client.SetAttribute("Interval", TimeValue(Seconds(0.0001))); // high rate
        client.SetAttribute("PacketSize", UintegerValue(payloadSize));

        ApplicationContainer clientApp = client.Install(stations.Get(i));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(simTime));
    }

    // Tracing & Logging (optional; users can set with NS_LOG)
    // Uncomment as needed:
    // phyHelpers[0].EnablePcap("wifiA", apDevs[0].Get(0));
    // phyHelpers[1].EnablePcap("wifiB", apDevs[1].Get(0));
    // phyHelpers[2].EnablePcap("wifiC", apDevs[2].Get(0));
    // phyHelpers[3].EnablePcap("wifiD", apDevs[3].Get(0));

    Simulator::Stop(Seconds(simTime + 2.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}