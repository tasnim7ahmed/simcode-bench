#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    for (int rtsCtsEnabled = 0; rtsCtsEnabled <= 1; ++rtsCtsEnabled) {
        NodeContainer nodes;
        nodes.Create(3);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        wifiMac.SetType("ns3::AdhocWifiMac");
        Ssid ssid = Ssid("adhoc-network");
        wifiMac.Set("Ssid", SsidValue(ssid));

        NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

        if (rtsCtsEnabled) {
            Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_Txop/RtsCtsThreshold", StringValue("100"));
        } else {
            Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_Txop/RtsCtsThreshold", StringValue("2200"));
        }

        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                       "MinX", DoubleValue(0.0),
                                       "MinY", DoubleValue(0.0),
                                       "DeltaX", DoubleValue(50.0),
                                       "DeltaY", DoubleValue(0.0),
                                       "GridWidth", UintegerValue(3),
                                       "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(nodes);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

        uint16_t port = 9;

        OnOffHelper onoff1("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), port)));
        onoff1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onoff1.SetAttribute("PacketSize", UintegerValue(1472));
        onoff1.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
        ApplicationContainer app1 = onoff1.Install(nodes.Get(0));
        app1.Start(Seconds(1.0));
        app1.Stop(Seconds(10.0));

        OnOffHelper onoff2("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), port)));
        onoff2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onoff2.SetAttribute("PacketSize", UintegerValue(1472));
        onoff2.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
        ApplicationContainer app2 = onoff2.Install(nodes.Get(2));
        app2.Start(Seconds(1.1));
        app2.Stop(Seconds(10.0));


        FlowMonitorHelper flowmon;
        Ptr<FlowMonitor> monitor = flowmon.InstallAll();

        Simulator::Stop(Seconds(10.0));
        Simulator::Run();

        monitor->CheckForLostPackets();

        Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
        FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

        std::cout << "RTS/CTS Enabled: " << (rtsCtsEnabled ? "Yes" : "No") << std::endl;
        for (auto i = stats.begin(); i != stats.end(); ++i) {
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
            std::cout << "  Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "   Tx Bytes:   " << i->second.txBytes << "\n";
            std::cout << "   Rx Bytes:   " << i->second.rxBytes << "\n";
            std::cout << "   Lost Packets: " << i->second.lostPackets << "\n";
            std::cout << "   Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
        }

        Simulator::Destroy();
    }

    return 0;
}