#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiAggregationExample");

int main(int argc, char* argv[]) {
    bool enableRtsCts = false;
    uint32_t payloadSize = 1472;
    double simulationTime = 10;
    double distance = 10;

    CommandLine cmd;
    cmd.AddValue("enableRtsCts", "Enable or disable RTS/CTS", enableRtsCts);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("distance", "Distance between AP and station", distance);
    cmd.Parse(argc, argv);

    NodeContainer apNodes;
    apNodes.Create(4);

    NodeContainer staNodes;
    staNodes.Create(4);

    YansWifiChannelHelper channel[4];
    WifiHelper wifi[4];
    YansWifiPhyHelper phy[4];
    WifiMacHelper mac[4];

    NetDeviceContainer staDevices[4];
    NetDeviceContainer apDevices[4];

    // Configure common parameters
    std::string phyMode("OfdmRate6MbpsBW10MHz");

    // Configure default channel settings
    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue(phyMode));

    for (int i = 0; i < 4; ++i) {
        // Configure channels.  Channels 36, 40, 44, 48 are non-overlapping in 5 GHz band.
        channel[i] = YansWifiChannelHelper::Default();
        channel[i].SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
        channel[i].AddPropagationLoss("ns3::FriisPropagationLossModel");

        phy[i] = YansWifiPhyHelper::Default();
        phy[i].SetChannel(channel[i].Create());
        phy[i].SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

        wifi[i] = WifiHelper::Default();
        wifi[i].SetStandard(WIFI_PHY_STANDARD_80211a);

        Ssid ssid = Ssid("ns3-wifi");
        mac[i] = WifiMacHelper::Default();
        mac[i].SetType("ns3::StaWifiMac",
                       "Ssid", SsidValue(ssid),
                       "ActiveProbing", BooleanValue(false));

        staDevices[i] = wifi[i].Install(phy[i], mac[i], staNodes.Get(i));

        mac[i].SetType("ns3::ApWifiMac",
                       "Ssid", SsidValue(ssid),
                       "BeaconGeneration", BooleanValue(true),
                       "BeaconInterval", TimeValue(Seconds(0.1)));

        apDevices[i] = wifi[i].Install(phy[i], mac[i], apNodes.Get(i));

        //Set channel number
        SsidValue ssidValue = SsidValue(ssid);
        Config::Set("/NodeList/" + std::to_string(apNodes.Get(i)->GetId()) + "/$ns3::WifiNetDevice/Mac/HwAddress", Mac48AddressValue(Mac48Address::Allocate()));
        Config::Set("/NodeList/" + std::to_string(apNodes.Get(i)->GetId()) + "/$ns3::WifiNetDevice/Mac/Ssid", ssidValue);
        Config::Set("/NodeList/" + std::to_string(apNodes.Get(i)->GetId()) + "/$ns3::WifiNetDevice/ChannelNumber", UintegerValue(36 + (i * 4)));  // Set different channels
    }

    // Configure aggregation

    // Station A: Default aggregation (A-MSDU disabled, A-MPDU enabled at 65 KB)
    Config::Set("/NodeList/" + std::to_string(staNodes.Get(0)->GetId()) + "/$ns3::WifiNetDevice/Mac/EdcaQueuePolicy/Queue/MaxAmpduSize", UintegerValue(65535));
    Config::Set("/NodeList/" + std::to_string(staNodes.Get(0)->GetId()) + "/$ns3::WifiNetDevice/Mac/EnableMsduAggregation", BooleanValue(false));

    // Station B: Both A-MPDU and A-MSDU disabled
    Config::Set("/NodeList/" + std::to_string(staNodes.Get(1)->GetId()) + "/$ns3::WifiNetDevice/Mac/EnableMpduAggregation", BooleanValue(false));
    Config::Set("/NodeList/" + std::to_string(staNodes.Get(1)->GetId()) + "/$ns3::WifiNetDevice/Mac/EnableMsduAggregation", BooleanValue(false));

    // Station C: A-MSDU enabled (8 KB), A-MPDU disabled
    Config::Set("/NodeList/" + std::to_string(staNodes.Get(2)->GetId()) + "/$ns3::WifiNetDevice/Mac/EnableMpduAggregation", BooleanValue(false));
    Config::Set("/NodeList/" + std::to_string(staNodes.Get(2)->GetId()) + "/$ns3::WifiNetDevice/Mac/EnableMsduAggregation", BooleanValue(true));
    Config::Set("/NodeList/" + std::to_string(staNodes.Get(2)->GetId()) + "/$ns3::WifiNetDevice/Mac/ fragmentationThreshold", UintegerValue(8192); //8KB
    Config::Set("/NodeList/" + std::to_string(staNodes.Get(2)->GetId()) + "/$ns3::WifiNetDevice/Mac/MaxAmsduSize", UintegerValue(8192)); //8KB

    // Station D: Two-level aggregation (A-MPDU: 32 KB, A-MSDU: 4 KB)
    Config::Set("/NodeList/" + std::to_string(staNodes.Get(3)->GetId()) + "/$ns3::WifiNetDevice/Mac/EdcaQueuePolicy/Queue/MaxAmpduSize", UintegerValue(32768)); // 32KB
    Config::Set("/NodeList/" + std::to_string(staNodes.Get(3)->GetId()) + "/$ns3::WifiNetDevice/Mac/EnableMsduAggregation", BooleanValue(true));
    Config::Set("/NodeList/" + std::to_string(staNodes.Get(3)->GetId()) + "/$ns3::WifiNetDevice/Mac/fragmentationThreshold", UintegerValue(4096); //4KB
    Config::Set("/NodeList/" + std::to_string(staNodes.Get(3)->GetId()) + "/$ns3::WifiNetDevice/Mac/MaxAmsduSize", UintegerValue(4096)); //4KB

    // Enable RTS/CTS
    if (enableRtsCts) {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(0));
    }

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(4),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNodes);

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(distance),
                                  "MinY", DoubleValue(distance),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(4),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);

    // Internet Stack
    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer staNodeInterfaces[4];
    Ipv4InterfaceContainer apNodeInterfaces[4];
    for (int i = 0; i < 4; ++i)
    {
        staNodeInterfaces[i] = address.Assign(staDevices[i]);
        apNodeInterfaces[i] = address.Assign(apDevices[i]);
        address.NewNetwork();
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Application: UDP Echo Client/Server
    uint16_t port = 9;

    UdpEchoServerHelper echoServer(port);

    ApplicationContainer serverApps[4];
    for (int i = 0; i < 4; ++i)
    {
        serverApps[i] = echoServer.Install(apNodes.Get(i));
        serverApps[i].Start(Seconds(0.0));
        serverApps[i].Stop(Seconds(simulationTime + 1));
    }

    UdpEchoClientHelper echoClient(apNodeInterfaces[0].GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(0xffffffff));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.00001))); // 10000 packets per second
    echoClient.SetAttribute("PacketSize", UintegerValue(payloadSize));

    ApplicationContainer clientApps[4];
    for (int i = 0; i < 4; ++i)
    {
        echoClient.SetRemote(apNodeInterfaces[i].GetAddress(0),port);
        clientApps[i] = echoClient.Install(staNodes.Get(i));
        clientApps[i].Start(Seconds(1.0));
        clientApps[i].Stop(Seconds(simulationTime));
    }

    // Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 1));

    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.txBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
    }

    Simulator::Destroy();

    return 0;
}