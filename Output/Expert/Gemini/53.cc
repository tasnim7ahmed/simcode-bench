#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/spectrum-module.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

uint32_t maxTxopDuration[4] = {0, 0, 0, 0};

void TxopCallback(std::string context, Time txop) {
    int networkId = std::stoi(context.substr(21, 1));
    maxTxopDuration[networkId] = std::max(maxTxopDuration[networkId], (uint32_t)txop.GetMilliSeconds());
}

int main(int argc, char *argv[]) {
    bool enableRtsCts = false;
    double distance = 10.0;
    uint32_t txopDuration = 0;

    CommandLine cmd;
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS (1) or disable (0)", enableRtsCts);
    cmd.AddValue("distance", "Distance between AP and STA (meters)", distance);
    cmd.AddValue("txopDuration", "TXOP Duration (microseconds)", txopDuration);
    cmd.Parse(argc, argv);

    NodeContainer apNodes[4];
    NodeContainer staNodes[4];
    NetDeviceContainer apDevices[4];
    NetDeviceContainer staDevices[4];
    Ipv4InterfaceContainer interfaces[4];

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel[4];
    for (int i = 0; i < 4; ++i) {
        channel[i] = YansWifiChannelHelper::Default();
        std::stringstream ss;
        ss << "channel-" << i;
        channel[i].SetId(ss.str());
        channel[i].AddPropagationLoss("ns3::FriisPropagationLossModel");
        phy.SetChannel(channel[i].Create());
    }

    WifiMacHelper mac;
    Ssid ssid[4];
    for (int i = 0; i < 4; ++i) {
        std::stringstream ss;
        ss << "network-" << i;
        ssid[i] = Ssid(ss.str());
    }

    phy.Set("ShortGuardEnabled", BooleanValue(true));

    apNodes[0].Create(1);
    staNodes[0].Create(1);
    apNodes[1].Create(1);
    staNodes[1].Create(1);
    apNodes[2].Create(1);
    staNodes[2].Create(1);
    apNodes[3].Create(1);
    staNodes[3].Create(1);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes[0]);
    mobility.Install(apNodes[1]);
    mobility.Install(apNodes[2]);
    mobility.Install(apNodes[3]);

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(distance),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.Install(staNodes[0]);
    mobility.Install(staNodes[1]);
    mobility.Install(staNodes[2]);
    mobility.Install(staNodes[3]);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid[0]),
                "BeaconGeneration", BooleanValue(true),
                "BeaconInterval", TimeValue(Seconds(0.1)));

    apDevices[0] = wifi.Install(phy, mac, apNodes[0]);
    apDevices[1] = wifi.Install(phy, mac, apNodes[1]);
    apDevices[2] = wifi.Install(phy, mac, apNodes[2]);
    apDevices[3] = wifi.Install(phy, mac, apNodes[3]);

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid[0]),
                "ActiveProbing", BooleanValue(false));

    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_QosTag", BooleanValue (true));

    staDevices[0] = wifi.Install(phy, mac, staNodes[0]);
    staDevices[1] = wifi.Install(phy, mac, staNodes[1]);
    staDevices[2] = wifi.Install(phy, mac, staNodes[2]);
    staDevices[3] = wifi.Install(phy, mac, staNodes[3]);

    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelNumber", UintegerValue(36));
    Config::Set("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelNumber", UintegerValue(40));
    Config::Set("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelNumber", UintegerValue(44));
    Config::Set("/NodeList/3/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelNumber", UintegerValue(48));

    if (enableRtsCts) {
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/RtsCtsThreshold", UintegerValue (0));
    }

    if (txopDuration > 0) {
        Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_Txop/MaxTxopDuration", TimeValue (MicroSeconds (txopDuration)));
    }

    Config::Set("/NodeList/4/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_AggregationMaxAmpduSize", UintegerValue(WifiMacHeader::AMPDU_MAX_LENGTH_65535));
    Config::Set("/NodeList/5/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_AggregationEnabled", BooleanValue(false));
    Config::Set("/NodeList/6/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_AggregationMaxMsduSize", UintegerValue(7935));
    Config::Set("/NodeList/6/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_AggregationEnabled", BooleanValue(true));
    Config::Set("/NodeList/6/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_AggregationAmpduEnabled", BooleanValue(false));
    Config::Set("/NodeList/6/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_AggregationMaxAmpduSize", UintegerValue(WifiMacHeader::AMPDU_MAX_LENGTH_8191));

    Config::Set("/NodeList/7/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_AggregationMaxAmpduSize", UintegerValue(WifiMacHeader::AMPDU_MAX_LENGTH_32767));
    Config::Set("/NodeList/7/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_AggregationMaxMsduSize", UintegerValue(4096));
    Config::Set("/NodeList/7/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_AggregationEnabled", BooleanValue(true));
    Config::Set("/NodeList/7/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_AggregationAmpduEnabled", BooleanValue(true));


    InternetStackHelper internet;
    internet.Install(apNodes[0]);
    internet.Install(staNodes[0]);
    internet.Install(apNodes[1]);
    internet.Install(staNodes[1]);
    internet.Install(apNodes[2]);
    internet.Install(staNodes[2]);
    internet.Install(apNodes[3]);
    internet.Install(staNodes[3]);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    interfaces[0] = address.Assign(staDevices[0]);
    interfaces[0].Add(address.Assign(apDevices[0]).GetAddress(0));

    address.SetBase("10.1.2.0", "255.255.255.0");
    interfaces[1] = address.Assign(staDevices[1]);
    interfaces[1].Add(address.Assign(apDevices[1]).GetAddress(0));

    address.SetBase("10.1.3.0", "255.255.255.0");
    interfaces[2] = address.Assign(staDevices[2]);
    interfaces[2].Add(address.Assign(apDevices[2]).GetAddress(0));

    address.SetBase("10.1.4.0", "255.255.255.0");
    interfaces[3] = address.Assign(staDevices[3]);
    interfaces[3].Add(address.Assign(apDevices[3]).GetAddress(0));

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(apNodes[0].Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    echoServer = UdpEchoServerHelper(9);
    serverApps = echoServer.Install(apNodes[1].Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    echoServer = UdpEchoServerHelper(9);
    serverApps = echoServer.Install(apNodes[2].Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    echoServer = UdpEchoServerHelper(9);
    serverApps = echoServer.Install(apNodes[3].Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));


    UdpEchoClientHelper echoClient(interfaces[0].GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10000000));
    echoClient.SetAttribute("Interval", TimeValue(MicroSeconds(1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(staNodes[0].Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    echoClient = UdpEchoClientHelper(interfaces[1].GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10000000));
    echoClient.SetAttribute("Interval", TimeValue(MicroSeconds(1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    clientApps = echoClient.Install(staNodes[1].Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    echoClient = UdpEchoClientHelper(interfaces[2].GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10000000));
    echoClient.SetAttribute("Interval", TimeValue(MicroSeconds(1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    clientApps = echoClient.Install(staNodes[2].Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    echoClient = UdpEchoClientHelper(interfaces[3].GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10000000));
    echoClient.SetAttribute("Interval", TimeValue(MicroSeconds(1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    clientApps = echoClient.Install(staNodes[3].Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    for(int i = 0; i < 4; i++){
        std::stringstream ss;
        ss << "/NodeList/" << i << "/DeviceList/*/$ns3::WifiNetDevice/Mac/Txop";
        Config::Connect(ss.str(), MakeBoundCallback(&TxopCallback, i));
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double throughput[4] = {0.0, 0.0, 0.0, 0.0};

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        for (int j = 0; j < 4; ++j) {
            if (t.destinationAddress == interfaces[j].GetAddress(0)) {
                throughput[j] = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
                break;
            }
        }
    }

    Simulator::Destroy();

    for (int i = 0; i < 4; ++i) {
        std::cout << "Network " << i << " Throughput: " << throughput[i] << " Mbps, Max TXOP Duration: " << maxTxopDuration[i] << " ms" << std::endl;
    }

    return 0;
}