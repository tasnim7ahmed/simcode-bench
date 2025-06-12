// wifi-aggregation-four-nets.cc

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiAggregationFourNets");

int main(int argc, char *argv[])
{
    // Default simulation parameters
    double simulationTime = 10.0; // seconds
    uint32_t payloadSize = 1472;  // bytes
    double distance = 5.0;        // meters
    bool enableRts = false;

    CommandLine cmd;
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("distance", "Distance between station and AP (meters)", distance);
    cmd.AddValue("enableRts", "Enable or disable RTS/CTS", enableRts);
    cmd.Parse(argc, argv);

    std::vector<uint16_t> channels = {36, 40, 44, 48};
    std::vector<std::string> netNames = {"A", "B", "C", "D"};

    // Aggregation settings for each network
    struct AggregationSettings
    {
        bool mpduEnable;
        uint32_t mpduSize;    // bytes
        bool msduEnable;
        uint32_t msduSize;    // bytes
    };

    std::vector<AggregationSettings> aggs =
    {
        {true, 65 * 1024, false, 0},   // Net A
        {false, 0, false, 0},          // Net B
        {false, 0, true, 8 * 1024},    // Net C
        {true, 32 * 1024, true, 4 * 1024} // Net D
    };

    NodeContainer staNodes[4];
    NodeContainer apNodes[4];
    NetDeviceContainer staDevices[4], apDevices[4];
    Ipv4InterfaceContainer staIfaces[4], apIfaces[4];

    MobilityHelper mobility;
    InternetStackHelper stack;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();

    // Set mobility: AP (0,0), STA (distance,0)
    for (int i = 0; i < 4; ++i)
    {
        staNodes[i].Create(1);
        apNodes[i].Create(1);

        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
        positionAlloc->Add(Vector(0.0, 0.0 + i * (distance + 5), 0.0));      // AP
        positionAlloc->Add(Vector(distance, 0.0 + i * (distance + 5), 0.0)); // STA
        mobility.SetPositionAllocator(positionAlloc);
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(apNodes[i]);
        mobility.Install(staNodes[i]);
    }

    // Install Internet stack
    for (int i = 0; i < 4; ++i)
    {
        stack.Install(staNodes[i]);
        stack.Install(apNodes[i]);
    }

    // Configure and install WiFi per network
    for (int i = 0; i < 4; ++i)
    {
        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211ac);

        if (enableRts)
        {
            wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                         "DataMode", StringValue("VhtMcs7"),
                                         "ControlMode", StringValue("VhtMcs0"),
                                         "RtsCtsThreshold", UintegerValue(0));
        }
        else
        {
            wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                         "DataMode", StringValue("VhtMcs7"),
                                         "ControlMode", StringValue("VhtMcs0"),
                                         "RtsCtsThreshold", UintegerValue(999999));
        }

        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());
        phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        // Set 5 GHz band channel width
        phy.Set("ChannelWidth", UintegerValue(20));

        WifiMacHelper mac;
        Ssid ssid = Ssid("network-" + netNames[i]);

        // Configure aggregation
        // Enable/disable A-MPDU and set size
        if (aggs[i].mpduEnable)
        {
            Config::SetDefault("ns3::WifiMacQueue::MaxAmpduSize", UintegerValue(aggs[i].mpduSize));
            wifi.Set("AmpduSupported", BooleanValue(true));
        }
        else
        {
            wifi.Set("AmpduSupported", BooleanValue(false));
        }

        // Enable/disable A-MSDU and set size
        if (aggs[i].msduEnable)
        {
            Config::SetDefault("ns3::WifiMacQueue::MaxAmsduSize", UintegerValue(aggs[i].msduSize));
            wifi.Set("AmsduSupported", BooleanValue(true));
        }
        else
        {
            wifi.Set("AmsduSupported", BooleanValue(false));
        }

        // Important: Set the channel number
        phy.Set("ChannelNumber", UintegerValue(channels[i]));

        // AP
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        apDevices[i] = wifi.Install(phy, mac, apNodes[i]);

        // STA
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));
        staDevices[i] = wifi.Install(phy, mac, staNodes[i]);
    }

    // Assign IP addresses per network
    for (int i = 0; i < 4; ++i)
    {
        Ipv4AddressHelper address;
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        staIfaces[i] = address.Assign(staDevices[i]);
        apIfaces[i] = address.Assign(apDevices[i]);
    }

    // Install applications: OnOffApplication on station, sink on AP
    for (int i = 0; i < 4; ++i)
    {
        uint16_t port = 5000 + i;

        OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(apIfaces[i].GetAddress(0), port)));
        onoff.SetConstantRate(DataRate("500Mbps"), payloadSize);
        onoff.SetAttribute("StartTime", TimeValue(Seconds(0.1)));
        onoff.SetAttribute("StopTime", TimeValue(Seconds(simulationTime)));

        onoff.Install(staNodes[i]);

        PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApp = sink.Install(apNodes[i]);
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(simulationTime + 1));
    }

    // Enable PCAP for debugging (optional)
    // for (int i = 0; i < 4; ++i)
    // {
    //     YansWifiPhyHelper::Default().EnablePcap("wifi-four-nets-ap-" + netNames[i], apDevices[i].Get(0));
    //     YansWifiPhyHelper::Default().EnablePcap("wifi-four-nets-sta-" + netNames[i], staDevices[i].Get(0));
    // }

    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}