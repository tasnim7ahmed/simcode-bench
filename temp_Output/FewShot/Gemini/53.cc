#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/command-line.h"

using namespace ns3;

double maxTxopDuration[4] = {0.0, 0.0, 0.0, 0.0};

void TxopDurationTracer(double duration, int apIndex) {
    if (duration > maxTxopDuration[apIndex]) {
        maxTxopDuration[apIndex] = duration;
    }
}

int main(int argc, char *argv[]) {
    bool enableRtsCts = false;
    double distance = 10.0;
    double txopLimit = 0.0;

    CommandLine cmd;
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS (0 or 1)", enableRtsCts);
    cmd.AddValue("distance", "Distance between AP and STA (meters)", distance);
    cmd.AddValue("txopLimit", "TXOP limit (microseconds)", txopLimit);
    cmd.Parse(argc, argv);

    NodeContainer aps[4];
    NodeContainer stas[4];
    NetDeviceContainer apDevs[4];
    NetDeviceContainer staDevs[4];

    for (int i = 0; i < 4; ++i) {
        aps[i].Create(1);
        stas[i].Create(1);
    }

    YansWifiChannelHelper channelHelper;
    YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();

    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211n);

    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue("DsssRate11Mbps"));

    WifiMacHelper macHelper;

    Ssid ssid1 = Ssid("network-0");
    Ssid ssid2 = Ssid("network-1");
    Ssid ssid3 = Ssid("network-2");
    Ssid ssid4 = Ssid("network-3");
    Ssid ssids[4] = {ssid1, ssid2, ssid3, ssid4};

    for (int i = 0; i < 4; ++i) {
        std::stringstream channelNumberStream;
        channelNumberStream << (36 + i * 4);
        std::string channelNumber = channelNumberStream.str();

        channelHelper.Set("Frequency", UintegerValue(36 + i * 4));
        std::stringstream ss;
        ss << "/NodeList/" << aps[i].Get(0)->GetId() << "/DeviceList/*/$ns3::WifiNetDevice/Mac/Txop";
        Config::ConnectWithoutContext(ss.str(), MakeCallback(&TxopDurationTracer, i));

        phyHelper.SetChannel(channelHelper.Create());

        macHelper.SetType("ns3::ApWifiMac",
                          "Ssid", SsidValue(ssids[i]),
                          "BeaconGeneration", BooleanValue(true),
                          "BeaconInterval", TimeValue(MilliSeconds(100)));
        apDevs[i] = wifiHelper.Install(phyHelper, macHelper, aps[i]);

        macHelper.SetType("ns3::StaWifiMac",
                          "Ssid", SsidValue(ssids[i]),
                          "ActiveProbing", BooleanValue(false));
        staDevs[i] = wifiHelper.Install(phyHelper, macHelper, stas[i]);
    }

    // Configure aggregation settings for each station
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_TxParams/AmpduEnable", BooleanValue(true));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_TxParams/MaxAmpduSize", UintegerValue(WifiMacHeader::AMPDU_MAX_LENGTH_64K));
    Config::Set("/NodeList/" + std::to_string(stas[1].Get(0)->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_TxParams/AmpduEnable", BooleanValue(false));
    Config::Set("/NodeList/" + std::to_string(stas[2].Get(0)->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_TxParams/AmpduEnable", BooleanValue(false));
    Config::Set("/NodeList/" + std::to_string(stas[2].Get(0)->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_TxParams/AmsduEnable", BooleanValue(true));
    Config::Set("/NodeList/" + std::to_string(stas[2].Get(0)->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_TxParams/MaxAmsduSize", UintegerValue(8192));
    Config::Set("/NodeList/" + std::to_string(stas[3].Get(0)->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_TxParams/AmpduEnable", BooleanValue(true));
    Config::Set("/NodeList/" + std::to_string(stas[3].Get(0)->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_TxParams/MaxAmpduSize", UintegerValue(32768));
    Config::Set("/NodeList/" + std::to_string(stas[3].Get(0)->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_TxParams/AmsduEnable", BooleanValue(true));
    Config::Set("/NodeList/" + std::to_string(stas[3].Get(0)->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_TxParams/MaxAmsduSize", UintegerValue(4096));

    if (enableRtsCts) {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/RtsCtsThreshold", UintegerValue(0));
    } else {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/RtsCtsThreshold", UintegerValue(2200));
    }

    if (txopLimit > 0) {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_TxParams/TxopLimit", TimeValue(MicroSeconds(txopLimit)));
    }

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(4),
                                  "LayoutType", StringValue("RowFirst"));

    for (int i = 0; i < 4; ++i) {
      NodeContainer tempNodes;
      tempNodes.Add(stas[i].Get(0));
      tempNodes.Add(aps[i].Get(0));
      mobility.Install(tempNodes);
      mobility.AssignStreams (tempNodes);
    }


    InternetStackHelper stack;
    for (int i = 0; i < 4; ++i) {
        stack.Install(aps[i]);
        stack.Install(stas[i]);
    }

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer apInterface[4];
    Ipv4InterfaceContainer staInterface[4];

    for (int i = 0; i < 4; ++i) {
        address.SetBase("10.1." + std::to_string(i) + ".0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(apDevs[i]);
        apInterface[i] = Ipv4InterfaceContainer(interfaces.Get(0));
        interfaces = address.Assign(staDevs[i]);
        staInterface[i] = Ipv4InterfaceContainer(interfaces.Get(0));
    }

    UdpClientHelper client(staInterface[0].GetAddress(0), 50000);
    client.SetAttribute("MaxPackets", UintegerValue(1000000));
    client.SetAttribute("Interval", TimeValue(MicroSeconds(1)));
    client.SetAttribute("PacketSize", UintegerValue(1472));

    ApplicationContainer clientApps[4];
    for (int i = 0; i < 4; ++i) {
        UdpServerHelper server(50000);
        ApplicationContainer serverApps = server.Install(aps[i].Get(0));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        client.SetRemote(apInterface[i].GetAddress(0), 50000);
        clientApps[i] = client.Install(stas[i].Get(0));
        clientApps[i].Start(Seconds(2.0));
        clientApps[i].Stop(Seconds(10.0));
    }

    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        double throughput = (i->second.rxBytes * 8.0) / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Throughput: " << throughput << " Mbps\n";

        for (int j = 0; j < 4; j++) {
            if (t.destinationAddress == apInterface[j].GetAddress(0)) {
                std::cout << "  Max TXOP Duration for AP " << j << ": " << maxTxopDuration[j] << " s\n";
                break;
            }
        }
    }

    Simulator::Destroy();
    return 0;
}