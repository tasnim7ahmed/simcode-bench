#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMcsThroughputExample");

struct ResultStats {
    uint32_t mcs;
    uint32_t channelWidth;
    Time guardInterval;
    double throughput_Mbps;
};

static void
PrintStats(const std::vector<ResultStats> &stats)
{
    std::cout << "MCS,ChannelWidth(MHz),GuardInterval(ns),Throughput(Mbps)\n";
    for (const auto &s : stats)
    {
        std::cout << s.mcs << "," << s.channelWidth << "," << s.guardInterval.GetNanoSeconds() << "," << s.throughput_Mbps << std::endl;
    }
}

int main(int argc, char *argv[])
{
    bool useSpectrum = false;
    double simulationTime = 10.0;
    double distance = 10.0;
    std::string phyModel = "YansWifiPhy";
    std::string errorModel = "ns3::NistErrorRateModel";
    uint32_t channelWidth = 20;
    std::string gi = "800ns"; // "800ns" or "400ns"
    std::string dataRate = "HtMcs7";
    std::string standard = "802.11n";
    uint32_t mcsMin = 0;
    uint32_t mcsMax = 7;
    std::string wifiType = "ns3::StaWifiMac";
    CommandLine cmd;
    cmd.AddValue("useSpectrum", "Use SpectrumWifiPhy if true, YansWifiPhy otherwise", useSpectrum);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("distance", "Distance between STA and AP (meters)", distance);
    cmd.AddValue("phyModel", "Physical Layer Type: YansWifiPhy or SpectrumWifiPhy", phyModel);
    cmd.AddValue("errorModel", "Error model to use", errorModel);
    cmd.AddValue("channelWidth", "Channel width in MHz", channelWidth);
    cmd.AddValue("gi", "Guard Interval: 800ns or 400ns", gi);
    cmd.AddValue("standard", "Wi-Fi standard: 802.11n, 802.11ac", standard);
    cmd.AddValue("mcsMin", "MCS index start", mcsMin);
    cmd.AddValue("mcsMax", "MCS index end (inclusive)", mcsMax);
    cmd.Parse(argc, argv);

    WifiStandard wifiStandard = WIFI_STANDARD_80211n;
    if (standard == "802.11n") wifiStandard = WIFI_STANDARD_80211n;
    else if (standard == "802.11ac") wifiStandard = WIFI_STANDARD_80211ac;
    else {
        NS_ABORT_MSG("Unsupported standard: " << standard);
    }

    Time guardInterval = (gi == "400ns" ? NanoSeconds(400) : NanoSeconds(800));
    std::vector<ResultStats> statsVec;

    for (uint32_t mcs = mcsMin; mcs <= mcsMax; ++mcs)
    {
        NodeContainer wifiApNode, wifiStaNode;
        wifiApNode.Create(1);
        wifiStaNode.Create(1);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        Ptr<YansWifiChannel> chan = channel.Create();

        NetDeviceContainer staDevice, apDevice;
        WifiHelper wifi;
        wifi.SetStandard(wifiStandard);

        WifiPhyHelper phy;
        if (useSpectrum || phyModel == "SpectrumWifiPhy") {
            SpectrumWifiPhyHelper spectrumPhy = SpectrumWifiPhyHelper::Default();
            Ptr<MultiModelSpectrumChannel> spectrumChan = CreateObject<MultiModelSpectrumChannel>();
            spectrumPhy.SetChannel(spectrumChan);
            spectrumPhy.Set("ChannelWidth", UintegerValue(channelWidth));
            spectrumPhy.Set("ShortGuardEnabled", BooleanValue(gi == "400ns"));
            // For compatibility
            phy = spectrumPhy;
        } else {
            phy = YansWifiPhyHelper::Default();
            phy.SetChannel(chan);
            phy.Set("ChannelWidth", UintegerValue(channelWidth));
            phy.Set("ShortGuardEnabled", BooleanValue(gi == "400ns"));
        }

        phy.SetErrorRateModel(errorModel);

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-wifi");

        mac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));

        HtWifiMacHelper htMacHelper;
        WifiMode mode;
        std::ostringstream oss;
        if (wifiStandard == WIFI_STANDARD_80211n)
            oss << "HtMcs" << mcs;
        else if (wifiStandard == WIFI_STANDARD_80211ac)
            oss << "VhtMcs" << mcs;
        else
            NS_ABORT_MSG("Unsupported standard configured");

        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                    "DataMode", StringValue(oss.str()),
                                    "ControlMode", StringValue(oss.str()));

        staDevice = wifi.Install(phy, mac, wifiStaNode);

        mac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));
        apDevice = wifi.Install(phy, mac, wifiApNode);

        // Mobility
        MobilityHelper mobility;
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
        positionAlloc->Add(Vector(0.0, 0.0, 0.0));        // AP
        positionAlloc->Add(Vector(distance, 0.0, 0.0));   // STA
        mobility.SetPositionAllocator(positionAlloc);
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(wifiApNode);
        mobility.Install(wifiStaNode);

        // Internet stack
        InternetStackHelper stack;
        stack.Install(wifiApNode);
        stack.Install(wifiStaNode);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer apInterface, staInterface;
        NetDeviceContainer devices;
        devices.Add(apDevice);
        devices.Add(staDevice);
        Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(apDevice, staDevice));

        // UDP traffic: OnOffApplication (STA -> AP)
        uint16_t port = 9;
        OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(0), port));
        onoff.SetConstantRate(DataRate("100Mbps"), 1400);
        onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
        onoff.SetAttribute("StopTime", TimeValue(Seconds(simulationTime)));

        ApplicationContainer app = onoff.Install(wifiStaNode.Get(0));

        PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApp = sink.Install(wifiApNode.Get(0));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(simulationTime + 1.0));

        // FlowMonitor
        FlowMonitorHelper flowHelper;
        Ptr<FlowMonitor> monitor = flowHelper.InstallAll();

        Simulator::Stop(Seconds(simulationTime + 1));
        Simulator::Run();

        double throughput = 0.0;
        monitor->CheckForLostPackets();
        Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
        std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
        for (const auto &iter : stats)
        {
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter.first);
            if (t.sourceAddress == interfaces.GetAddress(1) && t.destinationAddress == interfaces.GetAddress(0) && t.destinationPort == port)
            {
                double rxBytes = iter.second.rxBytes;
                double tPut = rxBytes * 8.0 / 1e6 / (simulationTime - 1.0); // ignore first second
                throughput = tPut;
            }
        }
        statsVec.push_back({mcs, channelWidth, guardInterval, throughput});

        Simulator::Destroy();
    }

    PrintStats(statsVec);

    return 0;
}