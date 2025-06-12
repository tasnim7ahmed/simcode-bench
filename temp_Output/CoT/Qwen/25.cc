#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wi-FiMCSAnalysis");

int main(int argc, char *argv[])
{
    uint32_t numStations = 4;
    double distance = 10.0; // meters
    uint32_t payloadSize = 1472;
    std::string dataRate = "100Mbps";
    bool enableRtsCts = false;
    bool useUlOfdma = true;
    bool use80Plus80 = false;
    bool useExtendedBlockAck = true;
    Time simulationTime = Seconds(10);
    bool useTcp = false;
    uint8_t mcsValue = 9;
    uint16_t channelWidth = 80;
    uint16_t guardInterval = 800;
    uint32_t band = 5;
    bool enablePcap = false;
    bool enableFlowMonitor = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numStations", "Number of stations", numStations);
    cmd.AddValue("distance", "Distance between station and AP (m)", distance);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("dataRate", "Data rate for UDP traffic", dataRate);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
    cmd.AddValue("useUlOfdma", "Use UL OFDMA", useUlOfdma);
    cmd.AddValue("use80Plus80", "Use 80+80 MHz channel width", use80Plus80);
    cmd.AddValue("useExtendedBlockAck", "Use extended block ack", useExtendedBlockAck);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("useTcp", "Use TCP instead of UDP", useTcp);
    cmd.AddValue("mcsValue", "MCS value to use", mcsValue);
    cmd.AddValue("channelWidth", "Channel width (MHz)", channelWidth);
    cmd.AddValue("guardInterval", "Guard interval duration (ns)", guardInterval);
    cmd.AddValue("band", "Wi-Fi frequency band: 2.4, 5 or 6 GHz", band);
    cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
    cmd.AddValue("enableFlowMonitor", "Enable Flow Monitor", enableFlowMonitor);

    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue("2200"));
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue(enableRtsCts ? "0" : "2200"));
    Config::SetDefault("ns3::WifiMacQueueItem::MaxPacketQueueSize", QueueSizeValue(QueueSize("100p")));
    Config::SetDefault("ns3::WifiPhy::ChannelSettings", StringValue(use80Plus80 ? "{0, 80, 0, 80}" : "{0, " + std::to_string(channelWidth) + ", 0}"));

    if (useExtendedBlockAck)
    {
        Config::SetDefault("ns3::HeConfiguration::MpduBufferSize", UintegerValue(256));
    }

    NodeContainer apNode;
    NodeContainer staNodes;
    apNode.Create(1);
    staNodes.Create(numStations);

    YansWifiPhyHelper phy;
    WifiHelper wifi;

    wifi.SetStandard(WIFI_STANDARD_80211ax);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HeMcs" + std::to_string(mcsValue)),
                                 "ControlMode", StringValue("HeMcs" + std::to_string(mcsValue)));

    WifiMacHelper mac;
    Ssid ssid = Ssid("wifi-mcs-analysis");

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "QosSupported", BooleanValue(true),
                "BeaconInterval", TimeValue(MicroSeconds(102400)),
                "EnableDownlinkMu", BooleanValue(true),
                "EnableUplinkMu", BooleanValue(useUlOfdma));

    phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    NetDeviceContainer apDevice;
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    apDevice = wifi.Install(phy, mac, apNode);

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0),
                                  "GridWidth", UintegerValue(10),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNodes);

    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface;
    Ipv4InterfaceContainer staInterfaces;

    apInterface = address.Assign(apDevice);
    staInterfaces = address.Assign(staDevices);

    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    if (useTcp)
    {
        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
        serverApps = sink.Install(staNodes.Get(0));
        serverApps.Start(Seconds(0.0));
        serverAds.Stop(simulationTime);

        OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(staInterfaces.GetAddress(0), 9));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onoff.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
        onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));

        clientApps.Add(onoff.Install(apNode));
        clientApps.Start(Seconds(1.0));
        clientAds.Stop(simulationTime - Seconds(1));
    }
    else
    {
        UdpEchoServerHelper echoServer(9);
        serverApps = echoServer.Install(staNodes.Get(0));
        serverAds.Start(Seconds(0.0));
        serverAds.Stop(simulationTime);

        UdpEchoClientHelper echoClient(staInterfaces.GetAddress(0), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue((simulationTime.GetSeconds() - 1) * 1000));
        echoClient.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
        echoClient.SetAttribute("PacketSize", UintegerValue(payloadSize));

        clientApps = echoClient.Install(apNode);
        clientApps.Start(Seconds(1.0));
        clientAds.Stop(simulationTime - Seconds(1));
    }

    if (enablePcap)
    {
        phy.EnablePcapAll("wifi-mcs-analysis", false);
    }

    if (enableFlowMonitor)
    {
        FlowMonitorHelper flowmon;
        Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    }

    Simulator::Stop(simulationTime);
    Simulator::Run();

    if (enableFlowMonitor)
    {
        monitor->CheckForLostPackets();
        monitor->SerializeToXmlFile("wifi-mcs-analysis.flowmon", false, false);
    }

    Simulator::Destroy();
    return 0;
}