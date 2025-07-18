#include "ns3/boolean.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/double.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/log.h"
#include "ns3/mobility-helper.h"
#include "ns3/rng-seed-manager.h"
#include "ns3/ssid.h"
#include "ns3/string.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/udp-server.h"
#include "ns3/uinteger.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-helper.h"

// This example considers two hidden stations in an 802.11n network which supports MPDU aggregation.
// The user can specify whether RTS/CTS is used and can set the number of aggregated MPDUs.
//
// Example: ./ns3 run "wifi-simple-ht-hidden-stations --enableRts=1 --nMpdus=8"
//
// Network topology:
//
//   Wifi 192.168.1.0
//
//        AP
//   *    *    *
//   |    |    |
//   n1   n2   n3
//
// Packets in this simulation belong to BestEffort Access Class (AC_BE).

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimplesHtHiddenStations");

int
main(int argc, char* argv[])
{
    uint32_t payloadSize{1472}; // bytes
    Time simulationTime{"10s"};
    uint32_t nMpdus{1};
    uint32_t maxAmpduSize{0};
    bool enableRts{false};
    double minExpectedThroughput{0};
    double maxExpectedThroughput{0};

    RngSeedManager::SetSeed(1);
    RngSeedManager::SetRun(7);

    CommandLine cmd(__FILE__);
    cmd.AddValue("nMpdus", "Number of aggregated MPDUs", nMpdus);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("enableRts", "Enable RTS/CTS", enableRts);
    cmd.AddValue("simulationTime", "Simulation time", simulationTime);
    cmd.AddValue("minExpectedThroughput",
                 "if set, simulation fails if the lowest throughput is below this value",
                 minExpectedThroughput);
    cmd.AddValue("maxExpectedThroughput",
                 "if set, simulation fails if the highest throughput is above this value",
                 maxExpectedThroughput);
    cmd.Parse(argc, argv);

    if (!enableRts)
    {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("999999"));
    }
    else
    {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("0"));
    }

    // Set the maximum size for A-MPDU with regards to the payload size
    maxAmpduSize = nMpdus * (payloadSize + 200);

    // Set the maximum wireless range to 5 meters in order to reproduce a hidden nodes scenario,
    // i.e. the distance between hidden stations is larger than 5 meters
    Config::SetDefault("ns3::RangePropagationLossModel::MaxRange", DoubleValue(5));

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    channel.AddPropagationLoss(
        "ns3::RangePropagationLossModel"); // wireless range limited to 5 meters!

    YansWifiPhyHelper phy;
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.SetChannel(channel.Create());
    phy.Set("ChannelSettings", StringValue("{36, 0, BAND_5GHZ, 0}"));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue("HtMcs7"),
                                 "ControlMode",
                                 StringValue("HtMcs0"));
    WifiMacHelper mac;

    Ssid ssid = Ssid("simple-mpdu-aggregation");
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid",
                SsidValue(ssid),
                "EnableBeaconJitter",
                BooleanValue(false));

    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_MaxAmpduSize",
                UintegerValue(maxAmpduSize));

    int64_t streamNumber = 20;
    streamNumber += WifiHelper::AssignStreams(apDevice, streamNumber);
    streamNumber += WifiHelper::AssignStreams(staDevices, streamNumber);

    // Setting mobility model
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

    // AP is between the two stations, each station being located at 5 meters from the AP.
    // The distance between the two stations is thus equal to 10 meters.
    // Since the wireless range is limited to 5 meters, the two stations are hidden from each other.
    positionAlloc->Add(Vector(5.0, 0.0, 0.0));
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(10.0, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);
    streamNumber += stack.AssignStreams(wifiApNode, streamNumber);
    streamNumber += stack.AssignStreams(wifiStaNodes, streamNumber);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer StaInterface;
    StaInterface = address.Assign(staDevices);
    Ipv4InterfaceContainer ApInterface;
    ApInterface = address.Assign(apDevice);

    // Setting applications
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiApNode);
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(simulationTime + Seconds(1.0));
    streamNumber += server.AssignStreams(wifiApNode, streamNumber);

    UdpClientHelper client(ApInterface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295U));
    client.SetAttribute("Interval", TimeValue(Time("0.0001"))); // packets/s
    client.SetAttribute("PacketSize", UintegerValue(payloadSize));

    // Saturated UDP traffic from stations to AP
    ApplicationContainer clientApp1 = client.Install(wifiStaNodes);
    clientApp1.Start(Seconds(1.0));
    clientApp1.Stop(simulationTime + Seconds(1.0));
    streamNumber += client.AssignStreams(wifiStaNodes, streamNumber);

    phy.EnablePcap("SimpleHtHiddenStations_Ap", apDevice.Get(0));
    phy.EnablePcap("SimpleHtHiddenStations_Sta1", staDevices.Get(0));
    phy.EnablePcap("SimpleHtHiddenStations_Sta2", staDevices.Get(1));

    AsciiTraceHelper ascii;
    phy.EnableAsciiAll(ascii.CreateFileStream("SimpleHtHiddenStations.tr"));

    Simulator::Stop(simulationTime + Seconds(1.0));

    Simulator::Run();

    double totalPacketsThrough = DynamicCast<UdpServer>(serverApp.Get(0))->GetReceived();

    Simulator::Destroy();

    auto throughput = totalPacketsThrough * payloadSize * 8 / simulationTime.GetMicroSeconds();
    std::cout << "Throughput: " << throughput << " Mbit/s" << '\n';
    if (throughput < minExpectedThroughput ||
        (maxExpectedThroughput > 0 && throughput > maxExpectedThroughput))
    {
        NS_LOG_ERROR("Obtained throughput " << throughput << " is not in the expected boundaries!");
        exit(1);
    }
    return 0;
}
