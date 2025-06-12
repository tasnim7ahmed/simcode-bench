#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/spectrum-module.h"
#include <fstream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wi-Fi80211axSimulation");

int main(int argc, char *argv[])
{
    uint32_t nStations = 5;
    double distance = 5.0; // meters
    std::string phyMode("HeMcs11");
    bool enableRtsCts = false;
    bool useUlOfdma = true;
    bool useExtendedBlockAck = true;
    bool use80Plus80 = false;
    uint32_t channelWidth = 80;
    uint32_t guardInterval = 800;
    uint32_t band = 5;
    uint32_t payloadSize = 1472; // bytes
    uint32_t protocol = 0; // 0 for UDP, 1 for TCP
    Time simulationTime = Seconds(10);
    bool verbose = false;
    bool pcapTracing = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nStations", "Number of stations", nStations);
    cmd.AddValue("distance", "Distance between stations and AP (m)", distance);
    cmd.AddValue("phyMode", "Wifi Phy mode (e.g., HeMcs11)", phyMode);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
    cmd.AddValue("useUlOfdma", "Use UL OFDMA", useUlOfdma);
    cmd.AddValue("useExtendedBlockAck", "Use Extended Block Ack", useExtendedBlockAck);
    cmd.AddValue("use80Plus80", "Use 80+80 MHz channels", use80Plus80);
    cmd.AddValue("channelWidth", "Channel width in MHz", channelWidth);
    cmd.AddValue("guardInterval", "Guard interval in ns", guardInterval);
    cmd.AddValue("band", "WiFi band: 2.4, 5 or 6 GHz", band);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("protocol", "Protocol type (0=UDP, 1=TCP)", protocol);
    cmd.AddValue("simulationTime", "Total simulation time", simulationTime);
    cmd.AddValue("verbose", "Turn on all log components", verbose);
    cmd.AddValue("pcapTracing", "Enable pcap tracing", pcapTracing);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("UdpClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServerApplication", LOG_LEVEL_INFO);
        LogComponentEnable("TcpSocketBase", LOG_LEVEL_INFO);
    }

    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nStations);

    YansWifiPhyHelper phy;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211_AX);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue(phyMode),
                                 "ControlMode", StringValue("HeMcs0"));

    NetDeviceContainer staDevices;
    NetDeviceContainer apDevice;

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    phy.Set("ChannelWidth", UintegerValue(channelWidth));
    phy.Set("ShortGuardIntervalSupported", BooleanValue(guardInterval == 800 ? true : false));
    phy.Set("PreambleDetectionSupported", BooleanValue(true));
    phy.Set("RxOversamplingFactor", DoubleValue(2.0));
    phy.Set("TxPowerStart", DoubleValue(20.0));
    phy.Set("TxPowerEnd", DoubleValue(20.0));
    phy.Set("TxPowerLevels", UintegerValue(1));

    if (use80Plus80)
    {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelBonding", EnumValue(WIFI_CHANNEL_BONDING_80_PLUS_80));
    }

    if (band == 2)
    {
        phy.Set("Frequency", UintegerValue(2412));
    }
    else if (band == 5)
    {
        phy.Set("Frequency", UintegerValue(5180));
    }
    else if (band == 6)
    {
        phy.Set("Frequency", UintegerValue(5965));
    }

    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconGeneration", BooleanValue(true),
                "BeaconInterval", TimeValue(Seconds(2.5)),
                "EnableDownlinkMuMimo", BooleanValue(useUlOfdma),
                "EnableUplinkOfdma", BooleanValue(useUlOfdma),
                "EnableRts", BooleanValue(enableRtsCts),
                "ExtendedBlockAck", BooleanValue(useExtendedBlockAck));

    apDevice = wifi.Install(phy, mac, wifiApNode);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0),
                                  "GridWidth", UintegerValue(nStations),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(0),
                                  "DeltaY", DoubleValue(0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.Install(wifiApNode);

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface;
    Ipv4InterfaceContainer staInterfaces;

    apInterface = address.Assign(apDevice);
    staInterfaces = address.Assign(staDevices);

    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    if (protocol == 0) // UDP
    {
        UdpServerHelper server(9);
        serverApps = server.Install(wifiApNode.Get(0));
        serverApps.Start(Seconds(0.0));
        serverApps.Stop(simulationTime);

        UdpClientHelper client(apInterface.GetAddress(0), 9);
        client.SetAttribute("MaxPackets", UintegerValue(4294967295));
        client.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
        client.SetAttribute("PacketSize", UintegerValue(payloadSize));

        clientApps = client.Install(wifiStaNodes);
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(simulationTime);
    }
    else // TCP
    {
        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
        serverApps = sink.Install(wifiApNode.Get(0));
        serverApps.Start(Seconds(0.0));
        serverApps.Stop(simulationTime);

        OnOffHelper client("ns3::TcpSocketFactory", InetSocketAddress(apInterface.GetAddress(0), 9));
        client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        client.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
        client.SetAttribute("PacketSize", UintegerValue(payloadSize));
        client.SetAttribute("MaxBytes", UintegerValue(0));

        clientApps = client.Install(wifiStaNodes);
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(simulationTime);
    }

    if (pcapTracing)
    {
        phy.EnablePcapAll("wifi-80211ax-sim", false);
    }

    Simulator::Stop(simulationTime);
    Simulator::Run();

    if (protocol == 0)
    {
        double throughput = 0;
        for (uint32_t i = 0; i < clientApps.GetN(); ++i)
        {
            Ptr<UdpClient> clientApp = DynamicCast<UdpClient>(clientApps.Get(i));
            throughput += clientApp->GetDataRate().CalculateTxRate(clientApp->GetSent(), simulationTime.GetSeconds());
        }
        NS_LOG_UNCOND("Throughput with MCS: " << phyMode << ", Channel Width: " << channelWidth << " MHz = " << throughput / (1024 * 1024) << " Mbps");
    }
    else
    {
        double throughput = 0;
        for (uint32_t i = 0; i < serverApps.GetN(); ++i)
        {
            Ptr<PacketSink> sinkApp = DynamicCast<PacketSink>(serverApps.Get(i));
            throughput += (sinkApp->GetTotalRx() * 8) / simulationTime.GetSeconds() / 1e6;
        }
        NS_LOG_UNCOND("Throughput with MCS: " << phyMode << ", Channel Width: " << channelWidth << " MHz = " << throughput << " Mbps");
    }

    Simulator::Destroy();
    return 0;
}