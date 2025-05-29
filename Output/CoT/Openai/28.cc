#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedWifiBgnHtComparison");

struct Scenario
{
    bool erpProtection;    // ERP Protection (mixed mode)
    bool shortPpdu;        // Short PPDU format (for N)
    bool shortSlot;        // Short slot time
    std::string wifiMode;  // "b/g" or "n/ht"
    uint32_t payloadSize;  // Bytes
    std::string transport; // "udp" or "tcp"
};

void PrintThroughput(Ptr<FlowMonitor> monitor, FlowMonitorHelper &flowmon, std::string scenarioDesc)
{
    double totalThroughput = 0;
    flowmon.CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto it = stats.begin(); it != stats.end(); ++it) {
        double txTime = it->second.timeLastTxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds();
        if (txTime == 0) continue;
        double throughput = (it->second.rxBytes * 8.0) / txTime / 1e6; // Mbps
        totalThroughput += throughput;
    }
    std::ofstream out ("throughput_results.txt", std::ios::app);
    out << scenarioDesc << "\t" << totalThroughput << " Mbps" << std::endl;
    out.close();
    std::cout << scenarioDesc << "\t" << totalThroughput << " Mbps" << std::endl;
}

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    std::vector<Scenario> scenarios = {
        // ERP Protection Mode,      Short PPDU,     Short Slot Time,      WiFi Mode,       Payload, Protocol
        // 1. 802.11b, normal slot, UDP, small packet
        { true, false, false, "b", 500, "udp"},
        // 2. 802.11g, short slot, TCP, medium packet
        { false, false, true, "g", 1400, "tcp"},
        // 3. 802.11n, HT, short ppdu, UDP, large packet
        { false, true, true, "n", 1472, "udp"},
        // 4. Mixed b/g ERP, normal slot, TCP, small packet
        { true, false, false, "b/g", 500, "tcp"},
        // 5. Mixed g/n, ERP off, short slot, UDP, medium packet
        { false, false, true, "g/n", 1000, "udp"},
    };

    for (const auto& sc : scenarios)
    {
        uint32_t nSta_b = 1, nSta_g = 1, nSta_n = 1;

        if (sc.wifiMode == "b")
            nSta_g = nSta_n = 0;
        else if (sc.wifiMode == "g")
            nSta_b = nSta_n = 0;
        else if (sc.wifiMode == "n")
            nSta_b = nSta_g = 0;
        else if (sc.wifiMode == "b/g")
            nSta_n = 0;
        else if (sc.wifiMode == "g/n")
            nSta_b = 0;

        NodeContainer wifiStaNodes_b, wifiStaNodes_g, wifiStaNodes_n, wifiApNode;
        wifiApNode.Create(1);
        wifiStaNodes_b.Create(nSta_b);
        wifiStaNodes_g.Create(nSta_g);
        wifiStaNodes_n.Create(nSta_n);

        // Mobility
        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue(0.0),
                                     "MinY", DoubleValue(0.0),
                                     "DeltaX", DoubleValue(5.0),
                                     "DeltaY", DoubleValue(5.0),
                                     "GridWidth", UintegerValue(5),
                                     "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(wifiStaNodes_b);
        mobility.Install(wifiStaNodes_g);
        mobility.Install(wifiStaNodes_n);
        mobility.Install(wifiApNode);

        // WiFi helpers for each mode
        WifiHelper wifi_b, wifi_g, wifi_n;
        wifi_b.SetStandard(WIFI_STANDARD_80211b);
        wifi_g.SetStandard(WIFI_STANDARD_80211g);
        wifi_n.SetStandard(WIFI_STANDARD_80211n);

        if (sc.shortSlot)
        {
            wifi_b.Set("ShortSlotTimeSupported", BooleanValue(true));
            wifi_g.Set("ShortSlotTimeSupported", BooleanValue(true));
            wifi_n.Set("ShortSlotTimeSupported", BooleanValue(true));
        }
        else
        {
            wifi_b.Set("ShortSlotTimeSupported", BooleanValue(false));
            wifi_g.Set("ShortSlotTimeSupported", BooleanValue(false));
            wifi_n.Set("ShortSlotTimeSupported", BooleanValue(false));
        }

        // ERP Protection
        YansWifiPhyHelper phy_b = YansWifiPhyHelper::Default();
        YansWifiPhyHelper phy_g = YansWifiPhyHelper::Default();
        YansWifiPhyHelper phy_n = YansWifiPhyHelper::Default();
        YansWifiChannelHelper chan = YansWifiChannelHelper::Default();
        phy_b.SetChannel(chan.Create());
        phy_g.SetChannel(chan.Create());
        phy_n.SetChannel(chan.Create());

        WifiMacHelper mac;

        Ssid ssid = Ssid("wifi-mixed");

        // Install b STAs
        NetDeviceContainer staDevices_b;
        if (nSta_b > 0)
        {
            mac.SetType("ns3::StaWifiMac",
                        "Ssid", SsidValue(ssid),
                        "QosSupported", BooleanValue(false));
            phy_b.Set("ShortPreambleSupported", BooleanValue(sc.shortPpdu));
            staDevices_b = wifi_b.Install(phy_b, mac, wifiStaNodes_b);
        }

        // Install g STAs
        NetDeviceContainer staDevices_g;
        if (nSta_g > 0)
        {
            mac.SetType("ns3::StaWifiMac",
                        "Ssid", SsidValue(ssid),
                        "QosSupported", BooleanValue(false));
            phy_g.Set("ShortPreambleSupported", BooleanValue(sc.shortPpdu));
            if (sc.erpProtection)
                wifi_g.Set("ErpSupported", BooleanValue(true));
            else
                wifi_g.Set("ErpSupported", BooleanValue(false));
            staDevices_g = wifi_g.Install(phy_g, mac, wifiStaNodes_g);
        }

        // Install n STAs
        NetDeviceContainer staDevices_n;
        if (nSta_n > 0)
        {
            mac.SetType("ns3::StaWifiMac",
                        "Ssid", SsidValue(ssid),
                        "QosSupported", BooleanValue(true));
            phy_n.Set("ShortGuardIntervalSupported", BooleanValue(sc.shortPpdu));
            wifi_n.Set("HtSupported", BooleanValue(true));
            staDevices_n = wifi_n.Install(phy_n, mac, wifiStaNodes_n);
        }

        // Install AP
        NetDeviceContainer apDevice;
        if (sc.wifiMode == "b")
        {
            mac.SetType("ns3::ApWifiMac",
                        "Ssid", SsidValue(ssid));
            apDevice = wifi_b.Install(phy_b, mac, wifiApNode);
        }
        else if (sc.wifiMode == "g")
        {
            mac.SetType("ns3::ApWifiMac",
                        "Ssid", SsidValue(ssid));
            apDevice = wifi_g.Install(phy_g, mac, wifiApNode);
        }
        else if (sc.wifiMode == "n")
        {
            mac.SetType("ns3::ApWifiMac",
                        "Ssid", SsidValue(ssid));
            apDevice = wifi_n.Install(phy_n, mac, wifiApNode);
        }
        else if (sc.wifiMode == "b/g")
        {
            mac.SetType("ns3::ApWifiMac",
                        "Ssid", SsidValue(ssid));
            apDevice = wifi_g.Install(phy_g, mac, wifiApNode);
        }
        else if (sc.wifiMode == "g/n")
        {
            mac.SetType("ns3::ApWifiMac",
                        "Ssid", SsidValue(ssid));
            apDevice = wifi_n.Install(phy_n, mac, wifiApNode);
        }

        // Aggregate all nodes and devices for Internet stack
        NodeContainer allNodes;
        NetDeviceContainer allDevs;
        allNodes.Add(wifiStaNodes_b);
        allNodes.Add(wifiStaNodes_g);
        allNodes.Add(wifiStaNodes_n);
        allNodes.Add(wifiApNode);
        allDevs.Add(staDevices_b);
        allDevs.Add(staDevices_g);
        allDevs.Add(staDevices_n);
        allDevs.Add(apDevice);

        // Internet stack
        InternetStackHelper stack;
        stack.Install(allNodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.0.0", "255.255.0.0");
        Ipv4InterfaceContainer interfaces = address.Assign(allDevs);

        ApplicationContainer apps;
        uint16_t port = 5000;

        // For each STA, install a sender app to AP, either UDP or TCP
        Address apLocalAddr(InetSocketAddress(interfaces.Get(allNodes.GetN() - 1), port));
        for (uint32_t i = 0; i < nSta_b + nSta_g + nSta_n; ++i)
        {
            if (sc.transport == "udp")
            {
                OnOffHelper onoff("ns3::UdpSocketFactory", apLocalAddr);
                onoff.SetAttribute("PacketSize", UintegerValue(sc.payloadSize));
                onoff.SetAttribute("DataRate", DataRateValue(DataRate("20Mbps")));
                onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
                onoff.SetAttribute("StopTime", TimeValue(Seconds(11.0)));
                onoff.SetAttribute("MaxBytes", UintegerValue(0));
                apps.Add(onoff.Install(allNodes.Get(i)));
                // Install packet sink at AP
                PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
                apps.Add(sink.Install(wifiApNode.Get(0)));
            }
            else if (sc.transport == "tcp")
            {
                BulkSendHelper bulksend("ns3::TcpSocketFactory", apLocalAddr);
                bulksend.SetAttribute("MaxBytes", UintegerValue(0));
                bulksend.SetAttribute("SendSize", UintegerValue(sc.payloadSize));
                bulksend.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
                bulksend.SetAttribute("StopTime", TimeValue(Seconds(11.0)));
                apps.Add(bulksend.Install(allNodes.Get(i)));
                // Install packet sink at AP
                PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
                apps.Add(sink.Install(wifiApNode.Get(0)));
            }
        }

        // Enable FlowMonitor
        FlowMonitorHelper flowmon;
        Ptr<FlowMonitor> monitor = flowmon.InstallAll();

        Simulator::Stop(Seconds(12.0));
        Simulator::Run();

        // Output throughput
        std::ostringstream scenarioDesc;
        scenarioDesc << "ERP:" << sc.erpProtection
                     << " ShortPPDU:" << sc.shortPpdu
                     << " ShortSlot:" << sc.shortSlot
                     << " WifiMode:" << sc.wifiMode
                     << " Payload:" << sc.payloadSize
                     << " Prot:" << sc.transport;
        PrintThroughput(monitor, flowmon, scenarioDesc.str());

        Simulator::Destroy();
    }

    std::cout << "Simulation Complete, throughput summary saved in throughput_results.txt" << std::endl;
    return 0;
}