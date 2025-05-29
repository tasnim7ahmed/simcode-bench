#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiHePerformanceTest");

void PrintResults(FlowMonitorHelper &flowmonHelper, Ptr<FlowMonitor> flowmon, std::string outputFileName)
{
    flowmon->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();
    double totalThr = 0;
    for (auto const &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        double throughput = (flow.second.rxBytes * 8.0) / (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds()) / 1e6;
        totalThr += throughput;
        std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "\tTx Packets: " << flow.second.txPackets << "\n";
        std::cout << "\tRx Packets: " << flow.second.rxPackets << "\n";
        std::cout << "\tThroughput: " << throughput << " Mbps\n";
    }
    std::cout << "Total throughput: " << totalThr << " Mbps\n";
    if (!outputFileName.empty())
    {
        std::ofstream out(outputFileName, std::ios_base::app);
        out << "Total throughput: " << totalThr << " Mbps\n";
        out.close();
    }
}

int main (int argc, char *argv[])
{
    // Configurable parameters
    uint32_t nSta = 4;
    double distance = 10.0;
    uint16_t payloadSize = 1472;
    std::string phyModel = "Yans"; // "Yans" or "Spectrum"
    std::string flavor = "6GHz"; // "2.4GHz", "5GHz", "6GHz"
    std::string access = "DL_MU"; // "DL_MU", "UL_OFDMA", "SU"
    std::string dataRate = "150Mbps";
    bool enableRts = false;
    bool enable80_80MHz = false;
    bool enableEBAck = false;
    bool enableUlOfdma = false;
    std::string appType = "UDP"; // "UDP", "TCP"
    uint8_t mcs = 5;
    uint16_t channelWidth = 80;
    std::string gi = "HE_0_8us"; // "HE_0_8us", "HE_1_6us", "HE_3_2us"
    double simTime = 10.0;

    CommandLine cmd;
    cmd.AddValue("nSta", "Number of stations", nSta);
    cmd.AddValue("distance", "Distance between AP and STAs (meters)", distance);
    cmd.AddValue("payloadSize", "Application packet size (bytes)", payloadSize);
    cmd.AddValue("phyModel", "PHY model: Yans or Spectrum", phyModel);
    cmd.AddValue("flavor", "WiFi band: 2.4GHz, 5GHz or 6GHz", flavor);
    cmd.AddValue("access", "DL_MU, UL_OFDMA, or SU", access);
    cmd.AddValue("dataRate", "Application data rate", dataRate);
    cmd.AddValue("enableRts", "Enable RTS/CTS", enableRts);
    cmd.AddValue("enable8080", "Enable 80+80 MHz channels", enable80_80MHz);
    cmd.AddValue("enableEBAck", "Enable extended block ack", enableEBAck);
    cmd.AddValue("enableUlOfdma", "Enable UL OFDMA", enableUlOfdma);
    cmd.AddValue("appType", "UDP or TCP", appType);
    cmd.AddValue("mcs", "HE MCS index (0-11)", mcs);
    cmd.AddValue("channelWidth", "Channel width (20/40/80/160)", channelWidth);
    cmd.AddValue("gi", "Guard interval (HE_0_8us, HE_1_6us, HE_3_2us)", gi);
    cmd.AddValue("simTime", "Simulation time (s)", simTime);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer wifiStaNodes, wifiApNode;
    wifiStaNodes.Create(nSta);
    wifiApNode.Create(1);

    // Mobility model
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 1.5));
    for (uint32_t i = 0; i < nSta; ++i)
        positionAlloc->Add(Vector(distance * (i + 1), 0.0, 1.5));

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNodes);

    // WiFi configuration
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ax);

    // Channel and PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    Ptr<YansWifiChannel> chan = channel.Create();

    // Set band
    double freq = 6.0e9;
    if (flavor == "2.4GHz")
        freq = 2.4e9;
    else if (flavor == "5GHz")
        freq = 5.0e9;

    // PHY
    WifiPhyHelper phy;
    if (phyModel == "Spectrum")
        phy = WifiPhyHelper::Default();
    else
        phy.SetChannel(chan);

    phy.Set("ChannelWidth", UintegerValue(channelWidth));
    phy.Set("Frequency", UintegerValue(uint32_t(freq/1e6)));
    phy.Set("GuardInterval", StringValue(gi));
    if(enable80_80MHz && channelWidth == 160)
        phy.Set("ChannelSettings", StringValue("80+80MHz"));
    if(flavor == "6GHz")
        phy.Set("Frequency", UintegerValue(5955)); // center freq MHz for 6GHz band

    // MAC and HE config
    WifiMacHelper mac;
    HtWifiMacHelper heMac = HtWifiMacHelper();
    Ssid ssid = Ssid("demo-ssid");

    NetDeviceContainer staDevices, apDevice;

    // STA/Client devices
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    phy.Set("RxSensitivity", DoubleValue(-92.0));
    phy.Set("TxPowerStart", DoubleValue(23.0));
    phy.Set("TxPowerEnd", DoubleValue(23.0));
    phy.Set("ShortGuardEnabled", BooleanValue(gi == "HE_0_8us"));
    mac.Set("BE_MaxAmpduLength", UintegerValue(65535));
    mac.Set("BE_MaxAmsduLength", UintegerValue(7935));
    mac.Set("BE_Rifs", BooleanValue(false));
    // Extended Block Ack
    if(enableEBAck)
        mac.Set("ExtendedBlockAck", BooleanValue(true));
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // AP device
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "EnableMultiUser", BooleanValue(access == "DL_MU" || enableUlOfdma));
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Set MCS (DL/UL)
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HeConfiguration/HeMcs", UintegerValue(mcs));
    Config::Set("/NodeList/*/DeviceList/*/RemoteStationManager/DataMode", StringValue("HeMcs" + std::to_string(mcs)));

    // Enable DL MU or UL OFDMA
    if (access == "DL_MU")
    {
        Config::Set("/NodeList/0/DeviceList/0/$ns3::WifiNetDevice/HeConfiguration/EnableDLMuMimo", BooleanValue(true));
    }
    if (enableUlOfdma || access == "UL_OFDMA")
    {
        Config::Set("/NodeList/0/DeviceList/0/$ns3::WifiNetDevice/HeConfiguration/EnableUlOfdma", BooleanValue(true));
    }

    // RTS/CTS
    if(enableRts)
    {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/RtsCtsThreshold", UintegerValue(0));
    }
    else
    {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/RtsCtsThreshold", UintegerValue(2347));
    }

    // Stack and IP assign
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);
    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.0.0");
    Ipv4InterfaceContainer staIfs = address.Assign(staDevices);
    Ipv4InterfaceContainer apIf = address.Assign(apDevice);

    // Install applications
    uint16_t port = 9;
    ApplicationContainer serverApp, clientApps;
    if (appType == "UDP")
    {
        UdpServerHelper udpServer(port);
        serverApp = udpServer.Install(wifiStaNodes);
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(simTime));

        UdpClientHelper udpClient;
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.0001)));
        udpClient.SetAttribute("MaxPackets", UintegerValue(4294967295u));
        udpClient.SetAttribute("PacketSize", UintegerValue(payloadSize));

        for (uint32_t i = 0; i < nSta; ++i)
        {
            udpClient.SetAttribute("RemoteAddress", AddressValue(staIfs.GetAddress(i)));
            udpClient.SetAttribute("RemotePort", UintegerValue(port));
            udpClient.SetAttribute("StartTime", TimeValue(Seconds(1.1)));
            udpClient.SetAttribute("StopTime", TimeValue(Seconds(simTime)));
            udpClient.SetAttribute("DataRate", StringValue(dataRate));
            clientApps.Add(udpClient.Install(wifiApNode.Get(0)));
        }
    }
    else // TCP
    {
        uint32_t pktSize = payloadSize;
        Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
        serverApp = packetSinkHelper.Install(wifiStaNodes);
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(simTime));

        OnOffHelper onoff("ns3::TcpSocketFactory", Address());
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onoff.SetAttribute("PacketSize", UintegerValue(pktSize));
        onoff.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
        for (uint32_t i = 0; i < nSta; ++i)
        {
            AddressValue remoteAddress(InetSocketAddress(staIfs.GetAddress(i), port));
            onoff.SetAttribute("Remote", remoteAddress);
            clientApps.Add(onoff.Install(wifiApNode.Get(0)));
        }
        clientApps.Start(Seconds(1.1));
        clientApps.Stop(Seconds(simTime));
    }

    // Flow monitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(simTime + 1));
    Simulator::Run();

    PrintResults(flowmonHelper, flowmon, "");

    Simulator::Destroy();
    return 0;
}