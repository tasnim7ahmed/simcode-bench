#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMixedNetworkExample");

struct ScenarioConfig
{
    bool erpProtection;  // ERP protection enabled/disabled
    bool shortSlotTime;  // Short slot time enabled/disabled
    bool shortPreamble;  // Short PPDU/preamble enabled/disabled
    uint32_t payloadSize;
    std::string protocol; // "UDP" or "TCP"
};

void PrintThroughput(FlowMonitorHelper& flowmonHelper, Ptr<FlowMonitor> monitor, const std::string& scenarioName)
{
    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    double sumThroughput = 0.0;
    for (const auto& kv : stats)
    {
        double throughput = kv.second.rxBytes * 8.0 / (kv.second.timeLastRxPacket.GetSeconds() - kv.second.timeFirstTxPacket.GetSeconds()) / 1000000.0; // Mbps
        sumThroughput += throughput;
    }
    std::cout << "Scenario: " << scenarioName << ", Avg Throughput: " << sumThroughput / stats.size() << " Mbps" << std::endl;
}

int main(int argc, char *argv[])
{
    LogComponentEnable("UdpClient", LOG_LEVEL_ERROR);
    LogComponentEnable("UdpServer", LOG_LEVEL_ERROR);
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_ERROR);
    LogComponentEnable("PacketSink", LOG_LEVEL_ERROR);

    std::vector<ScenarioConfig> scenarios = {
        // Various combinations as example; you can add more as needed:
        {true, true, false, 512, "UDP"},
        {true, false, false, 1472, "TCP"},
        {false, true, true, 1024, "UDP"},
        {false, false, true, 1500, "TCP"}
    };

    // For each scenario
    for (uint32_t scenarioIdx = 0; scenarioIdx < scenarios.size(); ++scenarioIdx)
    {
        ScenarioConfig config = scenarios[scenarioIdx];
        std::ostringstream scen;
        scen << "ERP=" << (config.erpProtection ? "1" : "0")
             << "_ShortSlot=" << (config.shortSlotTime ? "1" : "0")
             << "_ShortPre=" << (config.shortPreamble ? "1" : "0")
             << "_Payload=" << config.payloadSize
             << "_Proto=" << config.protocol;

        // Nodes: 1 AP, 2x 802.11b STA, 2x 802.11g STA, 2x 802.11n(2.4GHz, HT) STA
        NodeContainer wifiStaNodes_b, wifiStaNodes_g, wifiStaNodes_n, wifiApNode;
        wifiStaNodes_b.Create(2);
        wifiStaNodes_g.Create(2);
        wifiStaNodes_n.Create(2);
        wifiApNode.Create(1);

        // WiFi Channel
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        // 802.11b configuration
        WifiHelper wifi_b;
        wifi_b.SetStandard(WIFI_STANDARD_80211b);
        wifi_b.SetRemoteStationManager("ns3::ConstantRateWifiManager","DataMode", StringValue("DsssRate11Mbps"), "ControlMode", StringValue("DsssRate5_5Mbps"));
        if (config.shortPreamble)
            wifi_b.SetMac("ShortPreambleSupported", BooleanValue(true));
        else
            wifi_b.SetMac("ShortPreambleSupported", BooleanValue(false));
        Mac48Address apMac = Mac48Address::Allocate();

        WifiMacHelper mac_b;
        Ssid ssid = Ssid("ns3-80211-mixed");
        mac_b.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

        NetDeviceContainer staDevices_b = wifi_b.Install(phy, mac_b, wifiStaNodes_b);

        // 802.11g configuration
        WifiHelper wifi_g;
        wifi_g.SetStandard(WIFI_STANDARD_80211g);
        wifi_g.SetRemoteStationManager("ns3::ConstantRateWifiManager","DataMode", StringValue("ErpOfdmRate54Mbps"), "ControlMode", StringValue("ErpOfdmRate12Mbps"));
        WifiMacHelper mac_g;
        mac_g.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
        NetDeviceContainer staDevices_g = wifi_g.Install(phy, mac_g, wifiStaNodes_g);

        // 802.11n (HT) configuration at 2.4GHz
        WifiHelper wifi_n;
        wifi_n.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);
        wifi_n.SetRemoteStationManager("ns3::ConstantRateWifiManager","DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));
        WifiMacHelper mac_n;
        HtWifiMacHelper htMacHelper;
        HtWifiMacHelper::AllowLegacyHtMode();
        mac_n.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
        NetDeviceContainer staDevices_n = wifi_n.Install(phy, mac_n, wifiStaNodes_n);

        // AP config (supports b/g/n)
        WifiHelper wifi_ap;
        wifi_ap.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);
        wifi_ap.SetRemoteStationManager("ns3::ConstantRateWifiManager","DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));
        WifiMacHelper mac_ap;
        // Set ERP protection
        mac_ap.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid),
                       "EnableErpProtection", BooleanValue(config.erpProtection));
        NetDeviceContainer apDevices = wifi_ap.Install(phy, mac_ap, wifiApNode);

        // Short slot, short preamble options (all devices)
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ShortSlotTimeSupported", BooleanValue(config.shortSlotTime));
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ShortPreambleSupported", BooleanValue(config.shortPreamble));

        // Mobility
        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(wifiStaNodes_b);
        mobility.Install(wifiStaNodes_g);
        mobility.Install(wifiStaNodes_n);
        mobility.Install(wifiApNode);
        for (uint32_t i = 0; i < wifiStaNodes_b.GetN(); ++i)
            wifiStaNodes_b.Get(i)->GetObject<MobilityModel>()->SetPosition(Vector(1 + i, 0, 0));
        for (uint32_t i = 0; i < wifiStaNodes_g.GetN(); ++i)
            wifiStaNodes_g.Get(i)->GetObject<MobilityModel>()->SetPosition(Vector(3 + i, 0, 0));
        for (uint32_t i = 0; i < wifiStaNodes_n.GetN(); ++i)
            wifiStaNodes_n.Get(i)->GetObject<MobilityModel>()->SetPosition(Vector(5 + i, 0, 0));
        wifiApNode.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(0, 0, 0));

        // Install internet stack
        InternetStackHelper stack;
        stack.Install(wifiStaNodes_b);
        stack.Install(wifiStaNodes_g);
        stack.Install(wifiStaNodes_n);
        stack.Install(wifiApNode);

        // Assign IP addresses
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer staIf_b = address.Assign(staDevices_b);
        address.NewNetwork();
        Ipv4InterfaceContainer staIf_g = address.Assign(staDevices_g);
        address.NewNetwork();
        Ipv4InterfaceContainer staIf_n = address.Assign(staDevices_n);
        address.NewNetwork();
        Ipv4InterfaceContainer apIf = address.Assign(apDevices);

        // Applications: from each STA to AP (downlink)
        uint16_t port = 5000;
        ApplicationContainer clientApps, serverApps;

        if (config.protocol == "UDP")
        {
            // UDP: UdpClient->UdpServer
            for (uint32_t i = 0; i < wifiStaNodes_b.GetN(); ++i)
            {
                UdpServerHelper server(port + i);
                serverApps.Add(server.Install(wifiApNode.Get(0)));

                UdpClientHelper client(apIf.GetAddress(0), port + i);
                client.SetAttribute("MaxPackets", UintegerValue(10000));
                client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
                client.SetAttribute("PacketSize", UintegerValue(config.payloadSize));
                clientApps.Add(client.Install(wifiStaNodes_b.Get(i)));
            }
            for (uint32_t i = 0; i < wifiStaNodes_g.GetN(); ++i)
            {
                UdpServerHelper server(port + wifiStaNodes_b.GetN() + i);
                serverApps.Add(server.Install(wifiApNode.Get(0)));

                UdpClientHelper client(apIf.GetAddress(0), port + wifiStaNodes_b.GetN() + i);
                client.SetAttribute("MaxPackets", UintegerValue(10000));
                client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
                client.SetAttribute("PacketSize", UintegerValue(config.payloadSize));
                clientApps.Add(client.Install(wifiStaNodes_g.Get(i)));
            }
            for (uint32_t i = 0; i < wifiStaNodes_n.GetN(); ++i)
            {
                UdpServerHelper server(port + wifiStaNodes_b.GetN() + wifiStaNodes_g.GetN() + i);
                serverApps.Add(server.Install(wifiApNode.Get(0)));

                UdpClientHelper client(apIf.GetAddress(0), port + wifiStaNodes_b.GetN() + wifiStaNodes_g.GetN() + i);
                client.SetAttribute("MaxPackets", UintegerValue(10000));
                client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
                client.SetAttribute("PacketSize", UintegerValue(config.payloadSize));
                clientApps.Add(client.Install(wifiStaNodes_n.Get(i)));
            }
        }
        else // TCP
        {
            for (uint32_t i = 0; i < wifiStaNodes_b.GetN(); ++i)
            {
                PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(apIf.GetAddress(0), port + i));
                serverApps.Add(sink.Install(wifiApNode.Get(0)));
                BulkSendHelper bulkSend("ns3::TcpSocketFactory", InetSocketAddress(apIf.GetAddress(0), port + i));
                bulkSend.SetAttribute("MaxBytes", UintegerValue(0));
                bulkSend.SetAttribute("SendSize", UintegerValue(config.payloadSize));
                clientApps.Add(bulkSend.Install(wifiStaNodes_b.Get(i)));
            }
            for (uint32_t i = 0; i < wifiStaNodes_g.GetN(); ++i)
            {
                PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(apIf.GetAddress(0), port + wifiStaNodes_b.GetN() + i));
                serverApps.Add(sink.Install(wifiApNode.Get(0)));
                BulkSendHelper bulkSend("ns3::TcpSocketFactory", InetSocketAddress(apIf.GetAddress(0), port + wifiStaNodes_b.GetN() + i));
                bulkSend.SetAttribute("MaxBytes", UintegerValue(0));
                bulkSend.SetAttribute("SendSize", UintegerValue(config.payloadSize));
                clientApps.Add(bulkSend.Install(wifiStaNodes_g.Get(i)));
            }
            for (uint32_t i = 0; i < wifiStaNodes_n.GetN(); ++i)
            {
                PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(apIf.GetAddress(0), port + wifiStaNodes_b.GetN() + wifiStaNodes_g.GetN() + i));
                serverApps.Add(sink.Install(wifiApNode.Get(0)));
                BulkSendHelper bulkSend("ns3::TcpSocketFactory", InetSocketAddress(apIf.GetAddress(0), port + wifiStaNodes_b.GetN() + wifiStaNodes_g.GetN() + i));
                bulkSend.SetAttribute("MaxBytes", UintegerValue(0));
                bulkSend.SetAttribute("SendSize", UintegerValue(config.payloadSize));
                clientApps.Add(bulkSend.Install(wifiStaNodes_n.Get(i)));
            }
        }

        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(11.0));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(11.0));

        // FlowMonitor
        FlowMonitorHelper flowmonHelper;
        Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

        Simulator::Stop(Seconds(12.0));
        Simulator::Run();

        PrintThroughput(flowmonHelper, monitor, scen.str());

        Simulator::Destroy();
    }
    return 0;
}