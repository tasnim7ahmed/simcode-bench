#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool enableRtsCts = false;
    uint32_t payloadSize = 1472;
    double simulationTime = 10;
    double distance = 10;

    CommandLine cmd;
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("distance", "Distance between AP and STA", distance);
    cmd.Parse(argc, argv);

    NodeContainer apNodes[4], staNodes[4];
    NetDeviceContainer apDevices[4], staDevices[4];
    YansWifiChannelHelper channel[4];
    YansWifiPhyHelper phy[4];
    WifiHelper wifi[4];

    std::string rate ("HtMcs7");
    std::string phyMode ("HT_PHY_MODE");

    for (int i = 0; i < 4; ++i) {
        apNodes[i].Create(1);
        staNodes[i].Create(1);

        channel[i] = YansWifiChannelHelper::Default();
        phy[i] = YansWifiPhyHelper::Default();
        phy[i].SetChannel(channel[i].Create());

        wifi[i] = WifiHelper::Default();
        wifi[i].SetRemoteStationManager("ns3::AarfWifiManager");

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-wifi");

        mac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid),
                    "BeaconInterval", TimeValue(Seconds(0.0001)));

        apDevices[i] = wifi[i].Install(phy[i], mac, apNodes[i]);

        mac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));

        staDevices[i] = wifi[i].Install(phy[i], mac, staNodes[i]);
    }

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(4),
                                  "LayoutType", StringValue("RowFirst"));
    NodeContainer allNodes;
    for (int i = 0; i < 4; i++) {
      allNodes.Add(staNodes[i]);
    }
    mobility.Install(allNodes);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    for (int i = 0; i < 4; i++) {
        mobility.Install(apNodes[i]);
    }

    for(int i=0; i<4; ++i){
      Ptr<ConstantPositionMobilityModel> am = apNodes[i].Get(0)->GetObject<ConstantPositionMobilityModel>();
      am->SetPosition(Vector(distance * (i+1), distance, 0.0));
    }

    InternetStackHelper internet;
    for (int i = 0; i < 4; ++i) {
        internet.Install(apNodes[i]);
        internet.Install(staNodes[i]);
    }

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces[4], staInterfaces[4];
    for (int i = 0; i < 4; ++i) {
        apInterfaces[i] = address.Assign(apDevices[i]);
        staInterfaces[i] = address.Assign(staDevices[i]);
        address.NewNetwork();
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;
    BulkSendHelper source[4];
    Address sinkAddress[4];

    for (int i = 0; i < 4; ++i) {
        sinkAddress[i] = InetSocketAddress(apInterfaces[i].GetAddress(0), port);
        source[i] = BulkSendHelper("ns3::TcpSocketFactory", sinkAddress[i]);
        source[i].SetAttribute("SendSize", UintegerValue(payloadSize));
        ApplicationContainer sourceApps = source[i].Install(staNodes[i]);
        sourceApps.Start(Seconds(1.0));
        sourceApps.Stop(Seconds(simulationTime + 1));

        PacketSinkHelper sink("ns3::TcpSocketFactory",
                             InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApps = sink.Install(apNodes[i]);
        sinkApps.Start(Seconds(0.0));
        sinkApps.Stop(Seconds(simulationTime + 1));
    }

    Config::Set ("/NodeList/*/$ns3::TcpL4Protocol/SocketType", StringValue ("ns3::TcpNewReno"));

    if (enableRtsCts) {
        Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MgtRatelimit", StringValue ("10000"));
        Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/RtsCtsThreshold", UintegerValue (0));
    }

    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/AmsduEnable", BooleanValue(false));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/AmpduEnable", BooleanValue(true));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/MaxAmpduLength", UintegerValue(65535));

    Config::Set ("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/AmsduEnable", BooleanValue(false));
    Config::Set ("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/AmpduEnable", BooleanValue(false));

    Config::Set ("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/AmsduEnable", BooleanValue(true));
    Config::Set ("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/AmpduEnable", BooleanValue(false));
    Config::Set("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/MaxAmsduLength", UintegerValue(8192));

   Config::Set ("/NodeList/3/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/AmsduEnable", BooleanValue(true));
   Config::Set ("/NodeList/3/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/AmpduEnable", BooleanValue(true));
   Config::Set("/NodeList/3/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/MaxAmsduLength", UintegerValue(4096));
   Config::Set("/NodeList/3/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/MaxAmpduLength", UintegerValue(32767));


    phy[0].Set ("ChannelNumber", UintegerValue (36));
    phy[1].Set ("ChannelNumber", UintegerValue (40));
    phy[2].Set ("ChannelNumber", UintegerValue (44));
    phy[3].Set ("ChannelNumber", UintegerValue (48));

    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 1));

    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
    }

    monitor->SerializeToXmlFile("wifi-aggregation.xml", true, true);

    Simulator::Destroy();
    return 0;
}