#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/propagation-module.h"

#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

static uint64_t g_totalRxBytesAP1 = 0;
static uint64_t g_totalRxBytesAP2 = 0;

static std::ofstream g_obssPdLogFile;

void ObssPdResetTrace(Ptr<const Packet> packet, double rssi)
{
    if (g_obssPdLogFile.is_open())
    {
        g_obssPdLogFile << Simulator::Now().GetSeconds() << "\t"
                        << packet->GetSize() << "\t"
                        << rssi << std::endl;
    }
}

void CalculateThroughput(std::string name, uint64_t totalBytes, double simTime)
{
    double throughputMbps = (totalBytes * 8.0) / (simTime * 1000000.0);
    std::cout << name << " Throughput: " << throughputMbps << " Mbps" << std::endl;
}

int main(int argc, char *argv[])
{
    double d1 = 10.0;
    double d2 = 10.0;
    double d3 = 50.0;
    double txPower = 20.0;
    double ccaEdThreshold = -82.0;
    double obssPdThreshold = -70.0;
    bool enableObssPd = true;
    uint32_t channelWidth = 20;
    double simTime = 10.0;
    uint32_t packetSize = 1024;
    std::string packetInterval = "0.01";

    CommandLine cmd(__FILE__);
    cmd.AddValue("d1", "Distance between STA1 and AP1 (m)", d1);
    cmd.AddValue("d2", "Distance between STA2 and AP2 (m)", d2);
    cmd.AddValue("d3", "Distance between AP1 and AP2 (m)", d3);
    cmd.AddValue("txPower", "Transmit power in dBm", txPower);
    cmd.AddValue("ccaEdThreshold", "CCA-ED threshold in dBm", ccaEdThreshold);
    cmd.AddValue("obssPdThreshold", "OBSS-PD threshold in dBm", obssPdThreshold);
    cmd.AddValue("enableObssPd", "Enable/Disable OBSS-PD (true/false)", enableObssPd);
    cmd.AddValue("channelWidth", "Channel width in MHz (20, 40, 80)", channelWidth);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
    cmd.AddValue("packetInterval", "Time interval between packets (e.g., 0.01s)", packetInterval);
    cmd.Parse(argc, argv);

    if (channelWidth != 20 && channelWidth != 40 && channelWidth != 80)
    {
        NS_FATAL_ERROR("Unsupported channel width. Please choose 20, 40, or 80 MHz.");
    }

    g_obssPdLogFile.open("obss-pd-reset-log.txt", std::ios_base::out | std::ios_base::trunc);
    if (!g_obssPdLogFile.is_open())
    {
        NS_FATAL_ERROR("Could not open obss-pd-reset-log.txt for writing.");
    }
    g_obssPdLogFile << "Time\tPacketSize\tRSSI" << std::endl;

    Config::SetDefault("ns3::HeWifiPhy::ObssPdEnable", BooleanValue(enableObssPd));
    Config::SetDefault("ns3::HeWifiPhy::ObssPdThreshold", DoubleValue(obssPdThreshold));
    Config::SetDefault("ns3::HeWifiPhy::CcaEdThreshold", DoubleValue(ccaEdThreshold));
    Config::SetDefault("ns3::HeWifiPhy::TxPowerStart", DoubleValue(txPower));
    Config::SetDefault("ns3::HeWifiPhy::TxPowerEnd", DoubleValue(txPower));

    NodeContainer apNodes;
    apNodes.Create(2);
    NodeContainer staNodes;
    staNodes.Create(2);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(d3, 0.0, 0.0));
    positionAlloc->Add(Vector(d1, 0.0, 0.0));
    positionAlloc->Add(Vector(d3 + d2, 0.0, 0.0));

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);
    mobility.Install(staNodes);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ax);

    YansWifiChannelHelper channel;
    channel.SetPropagationLossModel("ns3::LogDistancePropagationLossModel",
                                    "Exponent", DoubleValue(3.0));
    channel.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
    Ptr<YansWifiChannel> wifiChannel = channel.Create();

    YansWifiPhyHelper phy;
    phy.SetChannel(wifiChannel);
    phy.Set("ChannelWidth", UintegerValue(channelWidth));

    WifiMacHelper mac;
    NetDeviceContainer apDevices, staDevices;

    mac.SetType("ns3::HeWifiMac",
                "Ssid", SsidValue(Ssid("ns3-ax-1")),
                "ActiveProbing", BooleanValue(false),
                "QosSupported", BooleanValue(true),
                "ChannelWidth", UintegerValue(channelWidth));
    apDevices.Add(wifi.Install(phy, mac, apNodes.Get(0)));

    mac.SetType("ns3::HeWifiMac",
                "Ssid", SsidValue(Ssid("ns3-ax-2")),
                "ActiveProbing", BooleanValue(false),
                "QosSupported", BooleanValue(true),
                "ChannelWidth", UintegerValue(channelWidth));
    apDevices.Add(wifi.Install(phy, mac, apNodes.Get(1)));

    mac.SetType("ns3::HeWifiMac",
                "Ssid", SsidValue(Ssid("ns3-ax-1")),
                "ActiveProbing", BooleanValue(true),
                "QosSupported", BooleanValue(true),
                "ChannelWidth", UintegerValue(channelWidth));
    staDevices.Add(wifi.Install(phy, mac, staNodes.Get(0)));

    mac.SetType("ns3::HeWifiMac",
                "Ssid", SsidValue(Ssid("ns3-ax-2")),
                "ActiveProbing", BooleanValue(true),
                "QosSupported", BooleanValue(true),
                "ChannelWidth", UintegerValue(channelWidth));
    staDevices.Add(wifi.Install(phy, mac, staNodes.Get(1)));

    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    OnOffHelper onoff1("ns3::UdpSocketFactory", InetSocketAddress(apInterfaces.GetAddress(0), 9));
    onoff1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoff1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    onoff1.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff1.SetAttribute("DataRate", StringValue("100Mbps"));
    onoff1.SetAttribute("Interval", TimeValue(Time(packetInterval)));

    ApplicationContainer clientApps1 = onoff1.Install(staNodes.Get(0));
    clientApps1.Start(Seconds(1.0));
    clientApps1.Stop(Seconds(simTime + 0.1));

    PacketSinkHelper sink1("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
    ApplicationContainer serverApps1 = sink1.Install(apNodes.Get(0));
    serverApps1.Start(Seconds(0.0));
    serverApps1.Stop(Seconds(simTime + 0.1));

    OnOffHelper onoff2("ns3::UdpSocketFactory", InetSocketAddress(apInterfaces.GetAddress(1), 9));
    onoff2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoff2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    onoff2.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff2.SetAttribute("DataRate", StringValue("100Mbps"));
    onoff2.SetAttribute("Interval", TimeValue(Time(packetInterval)));

    ApplicationContainer clientApps2 = onoff2.Install(staNodes.Get(1));
    clientApps2.Start(Seconds(1.0));
    clientApps2.Stop(Seconds(simTime + 0.1));

    PacketSinkHelper sink2("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
    ApplicationContainer serverApps2 = sink2.Install(apNodes.Get(1));
    serverApps2.Start(Seconds(0.0));
    serverApps2.Stop(Seconds(simTime + 0.1));

    for (uint32_t i = 0; i < staDevices.GetN(); ++i)
    {
        Ptr<HeWifiPhy> hePhy = DynamicCast<HeWifiPhy>(staDevices.Get(i)->GetPhy());
        if (hePhy)
        {
            hePhy->TraceConnectWithoutContext("ObssPdReset", MakeCallback(&ObssPdResetTrace));
        }
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    Ptr<PacketSink> ap1Sink = DynamicCast<PacketSink>(serverApps1.Get(0));
    g_totalRxBytesAP1 = ap1Sink->GetTotalRxBytes();
    CalculateThroughput("AP1", g_totalRxBytesAP1, simTime);

    Ptr<PacketSink> ap2Sink = DynamicCast<PacketSink>(serverApps2.Get(0));
    g_totalRxBytesAP2 = ap2Sink->GetTotalRxBytes();
    CalculateThroughput("AP2", g_totalRxBytesAP2, simTime);

    Simulator::Destroy();
    g_obssPdLogFile.close();

    return 0;
}