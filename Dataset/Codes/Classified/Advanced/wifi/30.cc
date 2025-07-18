#include "ns3/boolean.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/log.h"
#include "ns3/mobility-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/ssid.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-helper.h"

// This is a simple example in order to show how to configure an IEEE 802.11n Wi-Fi network
// with multiple TOS. It outputs the aggregated UDP throughput, which depends on the number of
// stations, the HT MCS value (0 to 7), the channel width (20 or 40 MHz) and the guard interval
// (long or short). The user can also specify the distance between the access point and the
// stations (in meters), and can specify whether RTS/CTS is used or not.

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMultiTos");

int
main(int argc, char* argv[])
{
    uint32_t nWifi{4};
    Time simulationTime{"10s"};
    meter_u distance{1.0};
    uint16_t mcs{7};
    uint8_t channelWidth{20}; // MHz
    bool useShortGuardInterval{false};
    bool useRts{false};

    CommandLine cmd(__FILE__);
    cmd.AddValue("nWifi", "Number of stations", nWifi);
    cmd.AddValue("distance",
                 "Distance in meters between the stations and the access point",
                 distance);
    cmd.AddValue("simulationTime", "Simulation time", simulationTime);
    cmd.AddValue("useRts", "Enable/disable RTS/CTS", useRts);
    cmd.AddValue("mcs", "MCS value (0 - 7)", mcs);
    cmd.AddValue("channelWidth", "Channel width in MHz", channelWidth);
    cmd.AddValue("useShortGuardInterval",
                 "Enable/disable short guard interval",
                 useShortGuardInterval);
    cmd.Parse(argc, argv);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nWifi);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);

    std::ostringstream oss;
    oss << "HtMcs" << mcs;
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue(oss.str()),
                                 "ControlMode",
                                 StringValue(oss.str()),
                                 "RtsCtsThreshold",
                                 UintegerValue(useRts ? 0 : 999999));

    Ssid ssid = Ssid("ns3-80211n");

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Set channel width
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelSettings",
                StringValue("{0, " + std::to_string(channelWidth) + ", BAND_2_4GHZ, 0}"));

    // Set guard interval
    Config::Set(
        "/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/ShortGuardIntervalSupported",
        BooleanValue(useShortGuardInterval));

    // mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    for (uint32_t i = 0; i < nWifi; i++)
    {
        positionAlloc->Add(Vector(distance, 0.0, 0.0));
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);
    Ipv4AddressHelper address;

    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staNodeInterfaces;
    Ipv4InterfaceContainer apNodeInterface;

    staNodeInterfaces = address.Assign(staDevices);
    apNodeInterface = address.Assign(apDevice);

    // Setting applications
    ApplicationContainer sourceApplications;
    ApplicationContainer sinkApplications;
    std::vector<uint8_t> tosValues = {0x70, 0x28, 0xb8, 0xc0}; // AC_BE, AC_BK, AC_VI, AC_VO
    uint32_t portNumber = 9;
    for (uint32_t index = 0; index < nWifi; ++index)
    {
        for (uint8_t tosValue : tosValues)
        {
            auto ipv4 = wifiApNode.Get(0)->GetObject<Ipv4>();
            const auto address = ipv4->GetAddress(1, 0).GetLocal();
            InetSocketAddress sinkSocket(address, portNumber++);
            OnOffHelper onOffHelper("ns3::UdpSocketFactory", sinkSocket);
            onOffHelper.SetAttribute("OnTime",
                                     StringValue("ns3::ConstantRandomVariable[Constant=1]"));
            onOffHelper.SetAttribute("OffTime",
                                     StringValue("ns3::ConstantRandomVariable[Constant=0]"));
            onOffHelper.SetAttribute("DataRate", DataRateValue(50000000 / nWifi));
            onOffHelper.SetAttribute("PacketSize", UintegerValue(1472)); // bytes
            onOffHelper.SetAttribute("Tos", UintegerValue(tosValue));
            sourceApplications.Add(onOffHelper.Install(wifiStaNodes.Get(index)));
            PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", sinkSocket);
            sinkApplications.Add(packetSinkHelper.Install(wifiApNode.Get(0)));
        }
    }

    sinkApplications.Start(Seconds(0.0));
    sinkApplications.Stop(simulationTime + Seconds(1.0));
    sourceApplications.Start(Seconds(1.0));
    sourceApplications.Stop(simulationTime + Seconds(1.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(simulationTime + Seconds(1.0));
    Simulator::Run();

    double throughput = 0;
    for (uint32_t index = 0; index < sinkApplications.GetN(); ++index)
    {
        double totalPacketsThrough =
            DynamicCast<PacketSink>(sinkApplications.Get(index))->GetTotalRx();
        throughput += ((totalPacketsThrough * 8) / simulationTime.GetMicroSeconds()); // Mbit/s
    }

    Simulator::Destroy();

    if (throughput > 0)
    {
        std::cout << "Aggregated throughput: " << throughput << " Mbit/s" << std::endl;
    }
    else
    {
        std::cout << "Obtained throughput is 0!" << std::endl;
        exit(1);
    }

    return 0;
}
