#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MixedWifiPerformanceExample");

// Helper function to print results
void PrintThroughput(Ptr<FlowMonitor> monitor, FlowMonitorHelper &flowmonHelper)
{
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalUdpThroughput = 0;
    double totalTcpThroughput = 0;
    uint32_t udpFlows = 0;
    uint32_t tcpFlows = 0;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        double throughput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000.0;
        if (t.protocol == 17)
        {
            totalUdpThroughput += throughput;
            udpFlows++;
        }
        else if (t.protocol == 6)
        {
            totalTcpThroughput += throughput;
            tcpFlows++;
        }
        std::cout << "FlowID: " << i->first << " Src: " << t.sourceAddress << " Dst: " << t.destinationAddress
                  << " Protocol: " << (uint32_t)t.protocol
                  << " Throughput: " << throughput << " Mbps" << std::endl;
    }
    std::cout << "UDP aggregated throughput: " << totalUdpThroughput << " Mbps (" << udpFlows << " flows)" << std::endl;
    std::cout << "TCP aggregated throughput: " << totalTcpThroughput << " Mbps (" << tcpFlows << " flows)" << std::endl;
}

int main(int argc, char *argv[])
{
    uint32_t nBgSta = 2;    // 802.11b/g (ERP) stations (non-HT)
    uint32_t nHtSta = 2;    // 802.11n (HT) stations
    uint32_t payloadSize = 1472;
    uint32_t simulationTime = 10; // seconds
    bool useRts = false;
    bool erpProtection = true;
    bool shortSlot = true;
    bool shortPreamble = false;
    bool useUdp = true;

    CommandLine cmd;
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("useUdp", "True for UDP, False for TCP", useUdp);
    cmd.AddValue("useRts", "Enable RTS/CTS", useRts);
    cmd.AddValue("erpProtection", "Enable ERP protection (for b/g)", erpProtection);
    cmd.AddValue("shortSlot", "Enable short slot time", shortSlot);
    cmd.AddValue("shortPreamble", "Enable short preamble", shortPreamble);
    cmd.Parse(argc, argv);

    NodeContainer wifiApNode;
    wifiApNode.Create(1);
    NodeContainer bgStaNodes;
    bgStaNodes.Create(nBgSta);
    NodeContainer htStaNodes;
    htStaNodes.Create(nHtSta);

    // Set position
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0), "DeltaX", DoubleValue(2.0),
                                  "DeltaY", DoubleValue(2.0), "GridWidth", UintegerValue(5), "LayoutType", StringValue("RowFirst"));
    mobility.Install(bgStaNodes);
    mobility.Install(htStaNodes);

    // --- Configure Wi-Fi --- 
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    std::vector<NetDeviceContainer> staDevicesVector;
    NetDeviceContainer apDevices;

    // 802.11b/g (ERP) (non-HT) stations
    WifiHelper wifiBg;
    wifiBg.SetStandard(WIFI_STANDARD_80211g);
    if (erpProtection)
    {
        wifiBg.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                       "DataMode", StringValue("ErpOfdmRate6Mbps"),
                                       "ControlMode", StringValue("ErpOfdmRate6Mbps"),
                                       "NonErpProtectionMode", StringValue("cts-to-self"));
    }
    else
    {
        wifiBg.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                       "DataMode", StringValue("ErpOfdmRate6Mbps"),
                                       "ControlMode", StringValue("ErpOfdmRate6Mbps"),
                                       "NonErpProtectionMode", StringValue("none"));
    }
    phy.Set("ShortSlotTimeSupported", BooleanValue(shortSlot));
    phy.Set("ShortPreambleSupported", BooleanValue(shortPreamble));
    WifiMacHelper macBg;
    Ssid ssid = Ssid("wifi-mixed");

    macBg.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(ssid),
                  "ActiveProbing", BooleanValue(false));
    NetDeviceContainer bgStaDevices = wifiBg.Install(phy, macBg, bgStaNodes);
    staDevicesVector.push_back(bgStaDevices);

    // 802.11n (HT) stations
    WifiHelper wifiHt;
    wifiHt.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);
    wifiHt.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                   "DataMode", StringValue("HtMcs0"),
                                   "ControlMode", StringValue("HtMcs0"));
    WifiMacHelper macHt;
    macHt.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(ssid),
                  "ActiveProbing", BooleanValue(false));
    NetDeviceContainer htStaDevices = wifiHt.Install(phy, macHt, htStaNodes);
    staDevicesVector.push_back(htStaDevices);

    // AP configuration
    WifiMacHelper macAp;
    macAp.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDeviceBg = wifiBg.Install(phy, macAp, wifiApNode);
    NetDeviceContainer apDeviceHt = wifiHt.Install(phy, macAp, wifiApNode);
    apDevices.Add(apDeviceBg.Get(0)); // Both AP devices in one container
    apDevices.Add(apDeviceHt.Get(0));

    // --- Install TCP/IP stack ---
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(bgStaNodes);
    stack.Install(htStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.3.0", "255.255.255.0");
    NetDeviceContainer allStaDevices;
    for (auto &dev : staDevicesVector)
        allStaDevices.Add(dev);
    NetDeviceContainer allDevices = allStaDevices;
    allDevices.Add(apDevices);
    Ipv4InterfaceContainer allInterfaces = address.Assign(allDevices);

    Ipv4InterfaceContainer bgStaIf, htStaIf, apIf;
    for (uint32_t i = 0; i < nBgSta; ++i) bgStaIf.Add(allInterfaces.Get(i));
    for (uint32_t i = nBgSta; i < nBgSta + nHtSta; ++i) htStaIf.Add(allInterfaces.Get(i));
    apIf.Add(allInterfaces.Get(nBgSta + nHtSta)); // AP bg
    apIf.Add(allInterfaces.Get(nBgSta + nHtSta + 1)); // AP ht

    // --- Application Setups ---
    uint16_t port = 9;
    ApplicationContainer apps;
    double startTime = 1.0, stopTime = simulationTime-1.0;

    for (uint32_t s=0; s<nBgSta; ++s)
    {
        if (useUdp)
        {
            UdpClientHelper clientHelper(apIf.GetAddress(0), port);
            clientHelper.SetAttribute("MaxPackets", UintegerValue(1000000));
            clientHelper.SetAttribute("Interval", TimeValue(MicroSeconds(10000)));
            clientHelper.SetAttribute("PacketSize", UintegerValue(payloadSize));

            apps.Add(clientHelper.Install(bgStaNodes.Get(s)));

            UdpServerHelper serverHelper(port);
            apps.Add(serverHelper.Install(wifiApNode.Get(0)));
        }
        else
        {
            BulkSendHelper bulk("ns3::TcpSocketFactory", InetSocketAddress(apIf.GetAddress(0), port));
            bulk.SetAttribute("MaxBytes", UintegerValue(0));
            bulk.SetAttribute("SendSize", UintegerValue(payloadSize));
            apps.Add(bulk.Install(bgStaNodes.Get(s)));

            PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
            apps.Add(sink.Install(wifiApNode.Get(0)));
        }
    }
    for (uint32_t s=0; s<nHtSta; ++s)
    {
        if (useUdp)
        {
            UdpClientHelper clientHelper(apIf.GetAddress(0), port+1);
            clientHelper.SetAttribute("MaxPackets", UintegerValue(1000000));
            clientHelper.SetAttribute("Interval", TimeValue(MicroSeconds(10000)));
            clientHelper.SetAttribute("PacketSize", UintegerValue(payloadSize));

            apps.Add(clientHelper.Install(htStaNodes.Get(s)));

            UdpServerHelper serverHelper(port+1);
            apps.Add(serverHelper.Install(wifiApNode.Get(0)));
        }
        else
        {
            BulkSendHelper bulk("ns3::TcpSocketFactory", InetSocketAddress(apIf.GetAddress(0), port+1));
            bulk.SetAttribute("MaxBytes", UintegerValue(0));
            bulk.SetAttribute("SendSize", UintegerValue(payloadSize));
            apps.Add(bulk.Install(htStaNodes.Get(s)));

            PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port+1));
            apps.Add(sink.Install(wifiApNode.Get(0)));
        }
    }

    apps.Start(Seconds(startTime));
    apps.Stop(Seconds(stopTime));

    // --- Enable RTS/CTS ---
    if (useRts)
    {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("0"));
    }
    else
    {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("65535"));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // --- Flow Monitor ---
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    // --- Run simulation ---
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    PrintThroughput(monitor, flowmonHelper);

    Simulator::Destroy();
    return 0;
}