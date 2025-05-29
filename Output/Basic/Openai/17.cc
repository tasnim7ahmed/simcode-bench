#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MultiWifiAggrChannels");

int main(int argc, char *argv[])
{
    double distance = 5.0;
    double simTime = 10.0;
    uint32_t payloadSize = 1472;
    bool useRtsCts = false;

    CommandLine cmd;
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("distance", "Distance between STA and AP in meters", distance);
    cmd.AddValue("useRtsCts", "Enable/disable RTS/CTS", useRtsCts);
    cmd.Parse(argc, argv);

    NodeContainer wifiStaNodes[4];
    NodeContainer wifiApNodes[4];
    for (int i = 0; i < 4; ++i)
    {
        wifiStaNodes[i].Create(1);
        wifiApNodes[i].Create(1);
    }

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac);

    YansWifiPhyHelper phy[4];
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    for (int i = 0; i < 4; ++i)
    {
        phy[i] = YansWifiPhyHelper::Default();
        phy[i].SetChannel(channel.Create());
    }

    WifiMacHelper mac;

    std::vector<uint32_t> channels = {36, 40, 44, 48};

    Ssid ssids[4] = {
        Ssid("networkA"),
        Ssid("networkB"),
        Ssid("networkC"),
        Ssid("networkD")
    };

    WifiMacHelper staMac[4], apMac[4];

    NetDeviceContainer staDevices[4], apDevices[4];

    // Aggregation settings
    struct AggSetting
    {
        bool ampduEnable;
        uint32_t ampduSize; // bytes
        bool amsduEnable;
        uint32_t amsduSize; // bytes
    };

    AggSetting aggSettings[4] = {
        // Network A
        {true, 65535, false, 0},
        // Network B
        {false, 0, false, 0},
        // Network C
        {false, 0, true, 8192},
        // Network D
        {true, 32768, true, 4096}
    };

    for (int i = 0; i < 4; ++i)
    {
        // Wi-Fi Manager
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                     "DataMode", StringValue("VhtMcs9"),
                                     "ControlMode", StringValue("VhtMcs0"),
                                     "RtsCtsThreshold", UintegerValue(useRtsCts ? 0 : 2200));

        phy[i].Set("ChannelNumber", UintegerValue(channels[i]));

        // AP
        mac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssids[i]));

        apDevices[i] = wifi.Install(phy[i], mac, wifiApNodes[i]);

        // STA
        mac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssids[i]),
                    "ActiveProbing", BooleanValue(false));

        staDevices[i] = wifi.Install(phy[i], mac, wifiStaNodes[i]);

        // Aggregation:
        Config::Set("/NodeList/" + std::to_string(wifiStaNodes[i].Get(0)->GetId()) +
                    "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BE_EdcaTxopN/$" 
                    "ns3::EdcaTxopN/EnableAmpdu", BooleanValue(aggSettings[i].ampduEnable));

        Config::Set("/NodeList/" + std::to_string(wifiStaNodes[i].Get(0)->GetId()) +
                    "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BE_EdcaTxopN/$"
                    "ns3::EdcaTxopN/AmpduMaxSize", UintegerValue(aggSettings[i].ampduSize));

        Config::Set("/NodeList/" + std::to_string(wifiStaNodes[i].Get(0)->GetId()) +
                    "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BE_EdcaTxopN/$"
                    "ns3::EdcaTxopN/EnableAmsdu", BooleanValue(aggSettings[i].amsduEnable));

        Config::Set("/NodeList/" + std::to_string(wifiStaNodes[i].Get(0)->GetId()) +
                    "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BE_EdcaTxopN/$"
                    "ns3::EdcaTxopN/AmsduMaxSize", UintegerValue(aggSettings[i].amsduSize));
    }

    // Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (int i = 0; i < 4; ++i)
    {
        positionAlloc->Add(Vector(10.0 * i, 0, 0));      // AP
        positionAlloc->Add(Vector(10.0 * i, distance, 0)); // STA
    }
    mobility.SetPositionAllocator(positionAlloc);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    for (int i = 0; i < 4; ++i)
    {
        mobility.Install(wifiApNodes[i]);
        mobility.Install(wifiStaNodes[i]);
    }

    // Internet stack
    InternetStackHelper stack;
    for (int i = 0; i < 4; ++i)
    {
        stack.Install(wifiApNodes[i]);
        stack.Install(wifiStaNodes[i]);
    }

    // Assign IP addresses
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer staIfs[4], apIfs[4];
    std::vector<std::string> netBase = {"10.1.1.0", "10.1.2.0", "10.1.3.0", "10.1.4.0"};
    for (int i = 0; i < 4; ++i)
    {
        address.SetBase(netBase[i].c_str(), "255.255.255.0");
        staIfs[i] = address.Assign(staDevices[i]);
        apIfs[i] = address.Assign(apDevices[i]);
    }

    // Applications: Install an OnOffApplication (close to constant transmission)
    ApplicationContainer appCont;
    uint16_t port = 5000;
    for (int i = 0; i < 4; ++i)
    {
        OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(apIfs[i].GetAddress(0), port));
        onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
        onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        ApplicationContainer temp = onoff.Install(wifiStaNodes[i].Get(0));
        temp.Start(Seconds(0.1));
        temp.Stop(Seconds(simTime));
        appCont.Add(temp);

        PacketSinkHelper sink("ns3::UdpSocketFactory",
                              InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApp = sink.Install(wifiApNodes[i].Get(0));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(simTime));
        appCont.Add(sinkApp);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}