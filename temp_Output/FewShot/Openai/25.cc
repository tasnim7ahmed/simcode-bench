#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiAxMcsParameterSweepExample");

void
PrintThroughput(Ptr<FlowMonitor> monitor, FlowMonitorHelper* flowmonHelper)
{
    double sumThroughput = 0;
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter)
    {
        Ipv4FlowClassifier::FiveTuple t = flowmonHelper->GetClassifier()->FindFlow(iter->first);
        double throughput = iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1e6;
        NS_LOG_UNCOND("Flow " << iter->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")"
                              << " Throughput: " << throughput << " Mbps");
        sumThroughput += throughput;
    }
    NS_LOG_UNCOND("Total Throughput: " << sumThroughput << " Mbps");
}

int
main(int argc, char* argv[])
{
    uint32_t nSta = 4;
    double simulationTime = 10; // seconds
    double distance = 5.0; // meters
    std::string phyModel = "Yans";
    std::string band = "5"; // GHz band: 2.4, 5, or 6
    std::string appType = "UDP"; // "UDP" or "TCP"
    uint32_t payloadSize = 1460;
    double offeredLoadMbps = 100.0;
    bool enableRtsCts = false;
    bool enable80p80 = false;
    bool enableEba = false;
    bool enableUlOfdma = false;
    std::string guardInterval = "800ns"; // "800ns" or "400ns"
    uint8_t mcs = 5; // Default MCS
    uint16_t channelWidth = 40; // MHz

    CommandLine cmd;
    cmd.AddValue("nSta", "Number of stations", nSta);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("distance", "distance between AP and stations", distance);
    cmd.AddValue("phyModel", "PHY model: Yans or Spectrum", phyModel);
    cmd.AddValue("band", "Wi-Fi band: 2.4, 5, or 6 GHz", band);
    cmd.AddValue("appType", "UDP or TCP", appType);
    cmd.AddValue("payloadSize", "Application payload size (bytes)", payloadSize);
    cmd.AddValue("offeredLoadMbps", "Offered traffic load in Mbps per STA", offeredLoadMbps);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
    cmd.AddValue("enable80p80", "Enable 80+80 MHz channel", enable80p80);
    cmd.AddValue("enableEba", "Enable Extended Block Ack", enableEba);
    cmd.AddValue("enableUlOfdma", "Enable UL OFDMA", enableUlOfdma);
    cmd.AddValue("guardInterval", "Guard interval: 800ns or 400ns", guardInterval);
    cmd.AddValue("mcs", "IEEE 802.11ax MCS index", mcs);
    cmd.AddValue("channelWidth", "Channel width in MHz (20, 40, 80, 160)", channelWidth);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ax);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    Ptr<YansWifiChannel> phyChannel = channel.Create();

    WifiMacHelper mac;
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HeW_MCS" + std::to_string(mcs)),
                                 "ControlMode", StringValue("HeMcs0"));

    WifiPhyHelper phy;
    if (phyModel == "Spectrum")
    {
        phy = WifiPhyHelper::Default();
        phy.Set("SpectrumChannel", PointerValue(CreateObject<MultiModelSpectrumChannel>()));
    }
    else
    {
        phy = WifiPhyHelper::Default();
        phy.SetChannel(phyChannel);
    }

    if (band == "2.4")
        phy.Set("Frequency", UintegerValue(2412));
    else if (band == "5")
        phy.Set("Frequency", UintegerValue(5180));
    else
        phy.Set("Frequency", UintegerValue(5955));

    phy.Set("ChannelWidth", UintegerValue(channelWidth));
    if (enable80p80 && channelWidth == 160)
        phy.Set("Enable80p80MHz", BooleanValue(true));
    else
        phy.Set("Enable80p80MHz", BooleanValue(false));

    phy.Set("ShortGuardEnabled", BooleanValue(guardInterval == "400ns"));

    NodeContainer wifiApNode;
    wifiApNode.Create(1);
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nSta);

    Ssid ssid = Ssid("wifi-ax-example");

    // AP MAC
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "EnableBeaconJitter", BooleanValue(false));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // STA MAC
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    if (enableRtsCts)
    {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("0"));
    }

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    double angleStep = 360.0 / nSta;
    for (uint32_t i = 0; i < nSta; ++i)
    {
        double angle = i * angleStep * M_PI / 180.0;
        positionAlloc->Add(Vector(distance * std::cos(angle), distance * std::sin(angle), 0.0));
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNodes);

    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    uint16_t port = 9;

    ApplicationContainer serverApps, clientApps;

    if (appType == "UDP")
    {
        for (uint32_t i = 0; i < nSta; ++i)
        {
            UdpServerHelper server(port);
            ApplicationContainer serverApp = server.Install(wifiApNode.Get(0));
            serverApps.Add(serverApp);

            UdpClientHelper client(apInterface.GetAddress(0), port);
            client.SetAttribute("MaxPackets", UintegerValue(uint32_t(-1)));
            client.SetAttribute("Interval", TimeValue(Seconds(double(payloadSize * 8.0 / (offeredLoadMbps * 1e6)))));
            client.SetAttribute("PacketSize", UintegerValue(payloadSize));
            ApplicationContainer clientApp = client.Install(wifiStaNodes.Get(i));
            clientApps.Add(clientApp);

            serverApp.Start(Seconds(1.0));
            serverApp.Stop(Seconds(simulationTime));
            clientApp.Start(Seconds(2.0));
            clientApp.Stop(Seconds(simulationTime));
        }
    }
    else // TCP
    {
        TrafficControlHelper tch;
        tch.SetRootQueueDisc("ns3::FqCoDelQueueDisc");
        tch.Install(staDevices);
        tch.Install(apDevice);

        for (uint32_t i = 0; i < nSta; ++i)
        {
            BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(apInterface.GetAddress(0), port + i));
            source.SetAttribute("MaxBytes", UintegerValue(0));
            source.SetAttribute("SendSize", UintegerValue(payloadSize));
            ApplicationContainer sourceApps = source.Install(wifiStaNodes.Get(i));
            clientApps.Add(sourceApps);

            PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port + i));
            ApplicationContainer sinkApps = sink.Install(wifiApNode.Get(0));
            serverApps.Add(sinkApps);

            sourceApps.Start(Seconds(2.0));
            sourceApps.Stop(Seconds(simulationTime));
            sinkApps.Start(Seconds(1.0));
            sinkApps.Stop(Seconds(simulationTime));
        }
    }

    if (enableUlOfdma)
    {
        for (uint32_t i = 0; i < staDevices.GetN(); ++i)
        {
            Config::Set("/NodeList/" + std::to_string(wifiStaNodes.Get(i)->GetId()) +
                        "/DeviceList/0/$ns3::WifiNetDevice/HeConfiguration/OfdmaUlEnabled",
                        BooleanValue(true));
        }
        Config::Set("/NodeList/" + std::to_string(wifiApNode.Get(0)->GetId()) +
                    "/DeviceList/0/$ns3::WifiNetDevice/HeConfiguration/OfdmaDlEnabled",
                    BooleanValue(true));
    }

    if (enableEba)
    {
        Config::Set("/ChannelList/0/$ns3::YansWifiChannel/PropagationLossModel/ExtendedBlockAck", BooleanValue(true));
    }

    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    PrintThroughput(monitor, &flowmonHelper);

    Simulator::Destroy();
    return 0;
}