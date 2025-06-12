#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t payloadSize = 1472;
    double simulationTime = 10.0;
    double distance = 5.0;
    bool enableRtsCts = false;

    CommandLine cmd;
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("distance", "Distance between STA and AP (meters)", distance);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
    cmd.Parse(argc, argv);

    uint16_t channels[4] = {36, 40, 44, 48};

    NodeContainer aps, stas;
    aps.Create(4);
    stas.Create(4);

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(wifiChannel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac);

    std::vector<NetDeviceContainer> apDevices(4), staDevices(4);
    InternetStackHelper stack;
    MobilityHelper mobility;

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < 4; ++i) {
        positionAlloc->Add(Vector(10.0 * i, 0.0, 0.0));     // AP
        positionAlloc->Add(Vector(10.0 * i, distance, 0.0)); // STA
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(aps);
    mobility.Install(stas);

    for (uint32_t i = 0; i < 4; ++i) {
        phy.Set("ChannelNumber", UintegerValue(channels[i]));
        Ssid ssid = Ssid("net-" + std::to_string(channels[i]));

        // --- Aggregation Settings ---
        // A: default agg, A-MSDU off, A-MPDU on (65535)
        // B: both off
        // C: A-MSDU 8192 (on), A-MPDU off
        // D: A-MPDU on (32768), A-MSDU on (4096)

        mac.SetType("ns3::StaWifiMac",
            "Ssid", SsidValue(ssid),
            "ActiveProbing", BooleanValue(false));
        switch (i) {
        case 0: // A
            wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                "DataMode", StringValue("VhtMcs8"),
                "ControlMode", StringValue("VhtMcs0"));
            mac.Set("BE_MaxAmpduSize", UintegerValue(65535));
            mac.Set("BE_MaxAmsduSize", UintegerValue(0));
            mac.Set("BE_AmsduEnable", BooleanValue(false));
            mac.Set("BE_AmpduEnable", BooleanValue(true));
            break;
        case 1: // B
            wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                "DataMode", StringValue("VhtMcs8"),
                "ControlMode", StringValue("VhtMcs0"));
            mac.Set("BE_MaxAmpduSize", UintegerValue(0));
            mac.Set("BE_MaxAmsduSize", UintegerValue(0));
            mac.Set("BE_AmsduEnable", BooleanValue(false));
            mac.Set("BE_AmpduEnable", BooleanValue(false));
            break;
        case 2: // C
            wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                "DataMode", StringValue("VhtMcs8"),
                "ControlMode", StringValue("VhtMcs0"));
            mac.Set("BE_MaxAmpduSize", UintegerValue(0));
            mac.Set("BE_MaxAmsduSize", UintegerValue(8192));
            mac.Set("BE_AmsduEnable", BooleanValue(true));
            mac.Set("BE_AmpduEnable", BooleanValue(false));
            break;
        case 3: // D
            wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                "DataMode", StringValue("VhtMcs8"),
                "ControlMode", StringValue("VhtMcs0"));
            mac.Set("BE_MaxAmpduSize", UintegerValue(32768));
            mac.Set("BE_MaxAmsduSize", UintegerValue(4096));
            mac.Set("BE_AmsduEnable", BooleanValue(true));
            mac.Set("BE_AmpduEnable", BooleanValue(true));
            break;
        }

        staDevices[i] = wifi.Install(phy, mac, NodeContainer(stas.Get(i)));

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        apDevices[i] = wifi.Install(phy, mac, NodeContainer(aps.Get(i)));
    }

    // Rts/Cts setting
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold",
        UintegerValue(enableRtsCts ? 0 : 65535));

    for (uint32_t i = 0; i < 4; ++i) {
        stack.Install(aps.Get(i));
        stack.Install(stas.Get(i));
    }

    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> apIf(4), staIf(4);

    for (uint32_t i = 0; i < 4; ++i) {
        std::ostringstream subnet;
        subnet << "192.168." << (10 + i) << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        staIf[i] = address.Assign(staDevices[i]);
        apIf[i] = address.Assign(apDevices[i]);
    }

    uint16_t port = 5000;
    for (uint32_t i = 0; i < 4; ++i) {
        PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
            InetSocketAddress(apIf[i].GetAddress(0), port));
        ApplicationContainer sinkApp = sinkHelper.Install(aps.Get(i));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(simulationTime + 1));

        OnOffHelper onoff("ns3::UdpSocketFactory",
            InetSocketAddress(apIf[i].GetAddress(0), port));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
        onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
        onoff.SetAttribute("DataRate", StringValue("1000Mbps"));
        ApplicationContainer app = onoff.Install(stas.Get(i));
        app.Start(Seconds(1.0));
        app.Stop(Seconds(simulationTime));
    }

    Simulator::Stop(Seconds(simulationTime + 2));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}