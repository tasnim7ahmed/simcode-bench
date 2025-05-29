#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiAggregationDemo");

struct TxopStat
{
    Time maxTxop = Seconds(0);
};

void
TxopTraceCallback(uint32_t idx, Ptr<TxopStat> stat, Time txopDuration)
{
    if (txopDuration > stat->maxTxop)
    {
        stat->maxTxop = txopDuration;
    }
}

static double
CalculateThroughput(Ptr<Application> app)
{
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(app);
    if (!sink)
        return 0;
    // Throughput in Mbps
    uint64_t rxBytes = sink->GetTotalRx();
    return (rxBytes * 8.0) / (10e6); // 10 seconds, Mbps
}

int main(int argc, char *argv[])
{
    // User-configurable parameters
    double distance = 10.0;
    bool enableRtsCts = false;
    double txopMin = 0;
    double txopMax = 0;

    CommandLine cmd;
    cmd.AddValue("distance", "Distance between AP and STA (meters)", distance);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS mechanism", enableRtsCts);
    cmd.AddValue("txop", "TXOP duration in microseconds (0 = default)", txopMax);
    cmd.Parse(argc, argv);

    // WiFi channels (UNII-1, 5GHz)
    uint16_t channels[] = {36, 40, 44, 48};
    std::string ssids[] = {"networkA", "networkB", "networkC", "networkD"};
    const uint32_t nNets = 4;

    NodeContainer aps, stas;
    aps.Create(nNets);
    stas.Create(nNets);

    std::vector<NetDeviceContainer> apDevs(nNets), staDevs(nNets);
    std::vector<YansWifiChannelHelper> channelsH(nNets);
    std::vector<YansWifiPhyHelper> phys(nNets);
    InternetStackHelper stack;

    for (uint32_t i = 0; i < nNets; ++i)
    {
        stack.Install(aps.Get(i));
        stack.Install(stas.Get(i));
    }

    std::vector<Ptr<TxopStat>> txopStats(nNets);
    for (uint32_t i = 0; i < nNets; ++i)
    {
        channelsH[i] = YansWifiChannelHelper::Default();
        phys[i] = YansWifiPhyHelper::Default();
        phys[i].SetChannel(channelsH[i].Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                    "DataMode", StringValue("HtMcs7"),
                                    "ControlMode", StringValue("HtMcs0"));

        // Unique channel number for each network
        WifiMacHelper mac;
        Ssid ssid(ssids[i]);

        // --- Configure aggregation ---
        bool amsdu = false, ampdu = true;
        uint32_t maxAmsdu = 0, maxAmpdu = 65535;
        switch (i)
        {
        case 0: // station A: default A-MPDU 65kB
            ampdu = true;
            maxAmpdu = 65535;
            amsdu = false;
            maxAmsdu = 0;
            break;
        case 1: // station B: aggregation OFF
            ampdu = false;
            maxAmpdu = 0;
            amsdu = false;
            maxAmsdu = 0;
            break;
        case 2: // station C: A-MSDU enabled 8kB, A-MPDU OFF
            ampdu = false;
            maxAmpdu = 0;
            amsdu = true;
            maxAmsdu = 8192;
            break;
        case 3: // station D: AMPDU 32kB, AMSDU 4kB
            ampdu = true;
            maxAmpdu = 32768;
            amsdu = true;
            maxAmsdu = 4096;
            break;
        }

        // MAC configuration for STA
        mac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));
        mac.Set("BE_MaxAmpduSize", UintegerValue(maxAmpdu));
        mac.Set("BE_MaxAmsduSize", UintegerValue(maxAmsdu));
        mac.Set("BE_AmsduEnable", BooleanValue(amsdu));
        mac.Set("BE_AmpduEnable", BooleanValue(ampdu));
        // global settings for all TID/queue
        mac.Set("QosSupported", BooleanValue(true));

        staDevs[i] = wifi.Install(phys[i], mac, stas.Get(i));

        // MAC configuration for AP
        mac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid),
                    "QosSupported", BooleanValue(true));

        // Matching AP Mac config, aggregation enables on AP side for fairness
        mac.Set("BE_MaxAmpduSize", UintegerValue(maxAmpdu));
        mac.Set("BE_MaxAmsduSize", UintegerValue(maxAmsdu));
        mac.Set("BE_AmsduEnable", BooleanValue(amsdu));
        mac.Set("BE_AmpduEnable", BooleanValue(ampdu));
        apDevs[i] = wifi.Install(phys[i], mac, aps.Get(i));

        // Set channel number
        Config::Set("/NodeList/" + std::to_string(aps.Get(i)->GetId())
            + "/DeviceList/0/Phy/ChannelNumber", UintegerValue(channels[i]));
        Config::Set("/NodeList/" + std::to_string(stas.Get(i)->GetId())
            + "/DeviceList/0/Phy/ChannelNumber", UintegerValue(channels[i]));

        // Optional: adjust TXOP (only applies to qaptxops, so set for AP MAC)
        if (txopMax > 0)
        {
            Config::Set("/NodeList/" + std::to_string(aps.Get(i)->GetId()) + "/DeviceList/0/Mac/BE_TxopLimit", TimeValue(MicroSeconds(txopMax)));
            Config::Set("/NodeList/" + std::to_string(aps.Get(i)->GetId()) + "/DeviceList/0/Mac/BE_TxopMinLimit", TimeValue(MicroSeconds(txopMin)));
        }

        // Optional: enable/disable RTS/CTS
        if (enableRtsCts)
        {
            Config::Set("/NodeList/" + std::to_string(stas.Get(i)->GetId()) + "/DeviceList/0/Mac/BE_RtsCtsThreshold", UintegerValue(0));
        }
        else
        {
            Config::Set("/NodeList/" + std::to_string(stas.Get(i)->GetId()) + "/DeviceList/0/Mac/BE_RtsCtsThreshold", UintegerValue(99999));
        }
    }

    // Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < nNets; ++i) {
        // Place AP at (i*distance,0,0), STA at (i*distance + distance,0,0)
        positionAlloc->Add(Vector(i*distance*2, 0, 0));
    }
    for (uint32_t i = 0; i < nNets; ++i) {
        positionAlloc->Add(Vector(i*distance*2 + distance, 0, 0));
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(aps);
    mobility.Install(stas);

    // IP stack + routing
    Ipv4AddressHelper ipv4;
    std::vector<Ipv4InterfaceContainer> apIfs(nNets), staIfs(nNets);
    for (uint32_t i = 0; i < nNets; ++i)
    {
        ipv4.SetBase("10.1." + std::to_string(i + 1) + ".0", "255.255.255.0");
        apIfs[i] = ipv4.Assign(apDevs[i]);
        staIfs[i] = ipv4.Assign(staDevs[i]);
    }

    // Install Applications
    uint16_t port = 5000;
    std::vector<ApplicationContainer> sinkApps(nNets), srcApps(nNets);
    for (uint32_t i = 0; i < nNets; ++i)
    {
        // Use PacketSink at AP, BulkSend at STA
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                                    InetSocketAddress(apIfs[i].GetAddress(0), port));
        sinkApps[i] = sinkHelper.Install(aps.Get(i));
        sinkApps[i].Start(Seconds(0.0));
        sinkApps[i].Stop(Seconds(10.0));

        BulkSendHelper src("ns3::TcpSocketFactory",
                           InetSocketAddress(apIfs[i].GetAddress(0), port));
        src.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited
        src.SetAttribute("SendSize", UintegerValue(1472));
        srcApps[i] = src.Install(stas.Get(i));
        srcApps[i].Start(Seconds(1.0));
        srcApps[i].Stop(Seconds(10.0));
    }

    // Set up TXOP tracing from AP MACs
    for (uint32_t i = 0; i < nNets; ++i)
    {
        txopStats[i] = CreateObject<TxopStat>();
        std::string path = "/NodeList/" + std::to_string(aps.Get(i)->GetId()) + "/DeviceList/0/Mac/TxopTrace";
        Config::Connect(path, MakeBoundCallback(&TxopTraceCallback, i, txopStats[i]));
    }

    Simulator::Stop(Seconds(10.1));
    Simulator::Run();

    // Report results
    std::cout << std::fixed << std::setprecision(3);
    for (uint32_t i = 0; i < nNets; ++i)
    {
        double thr = CalculateThroughput(sinkApps[i].Get(0));
        std::cout << "Network " << char('A' + i) << " (Channel " << channels[i]
                  << "): Throughput = " << thr << " Mbps, Max TXOP = "
                  << txopStats[i]->maxTxop.GetMicroSeconds() << " us" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}