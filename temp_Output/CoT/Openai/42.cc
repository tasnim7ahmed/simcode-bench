#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HiddenStationMpduAggregation");

int main(int argc, char *argv[])
{
    uint32_t mpduCount = 32;
    bool rtsCts = false;
    uint32_t payloadSize = 1472;
    double duration = 10.0;
    double offeredThroughput = 20.0; // Mbps per station
    std::string phyMode("HtMcs7");

    CommandLine cmd;
    cmd.AddValue("mpduCount", "Number of MPDUs in A-MPDU (aggregation)", mpduCount);
    cmd.AddValue("rtsCts", "Enable RTS/CTS", rtsCts);
    cmd.AddValue("payloadSize", "UDP payload size in bytes", payloadSize);
    cmd.AddValue("duration", "Simulation time in seconds", duration);
    cmd.AddValue("offeredThroughput", "Offered data rate per station (Mbps)", offeredThroughput);
    cmd.Parse(argc, argv);

    NodeContainer wifiStaNodes, wifiApNode;
    wifiStaNodes.Create(2);
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());
    phy.Set("ShortGuardEnabled", BooleanValue(true));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

    WifiMacHelper mac;
    Ssid ssid = Ssid("hidden-ht-ssid");

    HtWifiMacHelper htMac = HtWifiMacHelper::Default();

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue(phyMode),
                                "ControlMode", StringValue(phyMode),
                                "RtsCtsThreshold", UintegerValue(rtsCts ? 0 : 9999));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, wifiApNode);

    // Enable MPDU aggregation
    for (uint32_t i = 0; i < staDevices.GetN(); ++i)
    {
        Ptr<WifiNetDevice> dev = DynamicCast<WifiNetDevice>(staDevices.Get(i));
        Ptr<EdcaTxopN> vo = dev->GetMac()->GetEdcaTxopN(AC_VO);
        if (vo)
        {
            vo->SetMaxAmsduSize(mpduCount * payloadSize);
            vo->SetEnableAmpdu(true);
            vo->SetMaxAmpduSize(mpduCount * payloadSize);
        }
        Ptr<EdcaTxopN> be = dev->GetMac()->GetEdcaTxopN(AC_BE);
        if (be)
        {
            be->SetMaxAmsduSize(mpduCount * payloadSize);
            be->SetEnableAmpdu(true);
            be->SetMaxAmpduSize(mpduCount * payloadSize);
        }
    }

    for (uint32_t i = 0; i < apDevices.GetN(); ++i)
    {
        Ptr<WifiNetDevice> dev = DynamicCast<WifiNetDevice>(apDevices.Get(i));
        Ptr<EdcaTxopN> vo = dev->GetMac()->GetEdcaTxopN(AC_VO);
        if (vo)
        {
            vo->SetMaxAmsduSize(mpduCount * payloadSize);
            vo->SetEnableAmpdu(true);
            vo->SetMaxAmpduSize(mpduCount * payloadSize);
        }
        Ptr<EdcaTxopN> be = dev->GetMac()->GetEdcaTxopN(AC_BE);
        if (be)
        {
            be->SetMaxAmsduSize(mpduCount * payloadSize);
            be->SetEnableAmpdu(true);
            be->SetMaxAmpduSize(mpduCount * payloadSize);
        }
    }

    // Mobility: create a hidden node scenario
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));        // Station 1
    positionAlloc->Add(Vector(300.0, 0.0, 0.0));      // Station 2 (hidden from Station 1)
    positionAlloc->Add(Vector(150.0, 0.0, 0.0));      // AP in the middle
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    // Restrict wifi range so that each station only hears AP (not the other station)
    Config::SetDefault("ns3::RangePropagationLossModel::MaxRange", DoubleValue(160.0));
    phy.Set("RxGain", DoubleValue(0)); // Range model will compute losses
    Ptr<YansWifiChannel> wifiChannel = StaticCast<YansWifiChannel>(phy.GetChannel());
    wifiChannel->AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(160.0));
    wifiChannel->SetPropagationDelayModel(CreateObject<ConstantSpeedPropagationDelayModel>());

    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterfaces;
    apInterfaces = address.Assign(apDevices);

    // UDP Application traffic from each STA to AP
    ApplicationContainer appContainer;
    uint16_t port = 4000;
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
    {
        OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(apInterfaces.GetAddress(0), port + i)));
        onoff.SetAttribute("DataRate", DataRateValue(DataRate(offeredThroughput * 1e6)));
        onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
        onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
        onoff.SetAttribute("StopTime", TimeValue(Seconds(duration - 0.1)));
        appContainer.Add(onoff.Install(wifiStaNodes.Get(i)));

        PacketSinkHelper sink("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port + i)));
        ApplicationContainer sinkApp = sink.Install(wifiApNode.Get(0));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(duration));
        appContainer.Add(sinkApp);
    }

    // Enable tracing
    phy.EnablePcapAll("hidden-station-mpdu");
    AsciiTraceHelper ascii;
    phy.EnableAsciiAll(ascii.CreateFileStream("hidden-station-mpdu.tr"));

    // Throughput calculation
    Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(appContainer.Get(1));
    Ptr<PacketSink> sink2 = DynamicCast<PacketSink>(appContainer.Get(3));
    double lastTotalRxBytes = 0;

    Simulator::Schedule(Seconds(duration - 0.2), [&]() {
        double throughput1 = sink1->GetTotalRx() * 8.0 / (duration - 1.0) / 1e6;
        double throughput2 = sink2->GetTotalRx() * 8.0 / (duration - 1.0) / 1e6;
        std::cout << "Throughput Station 1: " << throughput1 << " Mbps" << std::endl;
        std::cout << "Throughput Station 2: " << throughput2 << " Mbps" << std::endl;
        std::cout << "Total Throughput: " << (throughput1 + throughput2) << " Mbps" << std::endl;
    });

    Simulator::Stop(Seconds(duration));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}