#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FourNetworkWifiAggregation");

int main(int argc, char *argv[])
{
    uint32_t payloadSize = 1000;            // bytes
    double simulationTime = 10.0;           // seconds
    double distance = 1.0;                  // meters
    bool enableRtsCts = false;
    std::string phyMode("OfdmRate24Mbps");

    CommandLine cmd(__FILE__);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("distance", "Distance between station and AP in meters", distance);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS handshake", enableRtsCts);
    cmd.Parse(argc, argv);

    // Set up logging
    LogComponentEnable("FourNetworkWifiAggregation", LOG_LEVEL_INFO);

    // Create nodes for four independent networks (each with 1 AP + 1 STA)
    NodeContainer apNodes;
    NodeContainer staNodes;
    apNodes.Create(4);
    staNodes.Create(4);

    // Create channels
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));

    if (enableRtsCts)
    {
        wifi.Set("RtsCtsThreshold", UintegerValue(0));
    }
    else
    {
        wifi.Set("RtsCtsThreshold", UintegerValue(999999));
    }

    // Install MAC and PHY layers on each network with different channels
    NetDeviceContainer apDevices;
    NetDeviceContainer staDevices;

    for (uint8_t i = 0; i < 4; ++i)
    {
        WifiMacHelper mac;
        Ssid ssid = Ssid("network-" + std::to_string(i));

        // Set different channel numbers
        uint8_t channelNumber = 36 + i * 4;
        phy.Set("ChannelNumber", UintegerValue(channelNumber));

        // Configure aggregation settings per station
        if (i == 0) // Station A: default (A-MSDU off, A-MPDU enabled at 65KB)
        {
            Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/MaxAmpduSize", UintegerValue(65535));
            mac.SetType("ns3::StaWifiMac",
                        "Ssid", SsidValue(ssid),
                        "ActiveProbing", BooleanValue(false));
        }
        else if (i == 1) // Station B: both disabled
        {
            Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/MaxAmpduSize", UintegerValue(0));
            Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/MaxAmsduSize", UintegerValue(0));
            mac.SetType("ns3::StaWifiMac",
                        "Ssid", SsidValue(ssid),
                        "ActiveProbing", BooleanValue(false));
        }
        else if (i == 2) // Station C: A-MSDU enabled at 8KB, A-MPDU disabled
        {
            Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/MaxAmsduSize", UintegerValue(8192));
            Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/MaxAmpduSize", UintegerValue(0));
            mac.SetType("ns3::StaWifiMac",
                        "Ssid", SsidValue(ssid),
                        "ActiveProbing", BooleanValue(false));
        }
        else if (i == 3) // Station D: two-level aggregation (A-MPDU 32KB, A-MSDU 4KB)
        {
            Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/MaxAmpduSize", UintegerValue(32768));
            Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/MaxAmsduSize", UintegerValue(4096));
            mac.SetType("ns3::StaWifiMac",
                        "Ssid", SsidValue(ssid),
                        "ActiveProbing", BooleanValue(false));
        }

        // Install AP
        mac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));
        apDevices.Add(wifi.Install(phy, mac, apNodes.Get(i)));

        // Install STA
        staDevices.Add(wifi.Install(phy, mac, staNodes.Get(i)));
    }

    // Mobility model - set positions
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

    // Internet stack
    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer apInterfaces;
    Ipv4InterfaceContainer staInterfaces;

    for (uint8_t i = 0; i < 4; ++i)
    {
        std::string subnet = "10.0." + std::to_string(i) + ".0";
        address.SetBase(subnet.c_str(), "255.255.255.0");

        apInterfaces.Add(address.Assign(apDevices.Get(i)));
        staInterfaces.Add(address.Assign(staDevices.Get(i)));
    }

    // UDP Echo server setup
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(staNodes);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    // UDP Echo client setup
    UdpEchoClientHelper echoClient(Ipv4Address(), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.001)));
    echoClient.SetAttribute("PacketSize", UintegerValue(payloadSize));

    for (uint8_t i = 0; i < 4; ++i)
    {
        echoClient.SetAttribute("RemoteAddress", Ipv4AddressValue(staInterfaces.GetAddress(i)));
        ApplicationContainer clientApp = echoClient.Install(apNodes.Get(i));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(simulationTime - 1.0));
    }

    // Enable PCAP tracing
    phy.EnablePcapAll("four_network_wifi_aggregation");

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}