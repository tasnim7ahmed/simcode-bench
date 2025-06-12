#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool verbose = false;
    uint32_t nWifi = 1;
    double simulationTime = 10;
    std::string rate ("5Mbps");
    double distance = 10.0;
    bool rtscts = false;
    std::string protocol = "Udp";
    uint32_t packetSize = 1472;
    uint32_t port = 9;
    double expectedThroughput = 5;
    uint8_t mcs = 0;
    bool shortGuardInterval = false;
    uint32_t channelWidth = 20;

    CommandLine cmd;
    cmd.AddValue ("verbose", "Tell application to log if set to true", verbose);
    cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue ("rate", "CBR traffic rate", rate);
    cmd.AddValue ("distance", "Distance STA <--> AP", distance);
    cmd.AddValue ("rtscts", "Enable RTS/CTS", rtscts);
    cmd.AddValue ("protocol", "Transport protocol to use: Tcp or Udp", protocol);
    cmd.AddValue ("packetSize", "Size of packets generated. Default: 1472", packetSize);
    cmd.AddValue ("expectedThroughput", "Expected throughput", expectedThroughput);
    cmd.AddValue ("mcs", "MCS value (0-7)", mcs);
    cmd.AddValue ("shortGuardInterval", "Use short guard interval", shortGuardInterval);
    cmd.AddValue ("channelWidth", "Channel Width in MHz (20 or 40)", channelWidth);

    cmd.Parse (argc, argv);

    if (verbose) {
        LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

    Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue (rtscts ? "0" : "2200"));
    Config::SetDefault ("ns3::WifiMacQueue::MaxPackets", UintegerValue (1000));

    NodeContainer apNode;
    apNode.Create (1);
    NodeContainer staNodes;
    staNodes.Create (nWifi);

    WifiHelper wifiHelper;
    wifiHelper.SetStandard (WIFI_PHY_STANDARD_80211n);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    wifiPhy.Set ("RxGain", DoubleValue (-10));
    wifiPhy.Set ("ChannelWidth", UintegerValue (channelWidth));
    wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
    wifiPhy.SetChannel (wifiChannel.Create ());

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid ("ns-3-ssid");
    wifiMac.SetType ("ns3::ApWifiMac",
                    "Ssid", SsidValue (ssid),
                    "BeaconInterval", TimeValue (MicroSeconds (102400)),
                    "QosCategory", EnumValue (AC_BE),
                    "BE_TxopLimit", TimeValue (MilliSeconds (0)),
                    "CWmin", UintegerValue (16),
                    "CWmax", UintegerValue (1023));

    NetDeviceContainer apDevice = wifiHelper.Install (wifiPhy, wifiMac, apNode);

    wifiMac.SetType ("ns3::StaWifiMac",
                    "Ssid", SsidValue (ssid),
                    "QosCategory", EnumValue (AC_BE),
                    "BE_TxopLimit", TimeValue (MilliSeconds (0)),
                    "CWmin", UintegerValue (16),
                    "CWmax", UintegerValue (1023));

    NetDeviceContainer staDevices = wifiHelper.Install (wifiPhy, wifiMac, staNodes);

    if (shortGuardInterval) {
        Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ShortGuardIntervalSupported", BooleanValue (true));
    }

    wifiHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                        "DataMode", StringValue ("HtMcs" + std::to_string(mcs)),
                                        "ControlMode", StringValue ("HtMcs" + std::to_string(mcs)));

    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue (0.0),
                                   "MinY", DoubleValue (0.0),
                                   "DeltaX", DoubleValue (distance),
                                   "DeltaY", DoubleValue (0.0),
                                   "GridWidth", UintegerValue (3),
                                   "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (staNodes);

    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue (0.0),
                                   "MinY", DoubleValue (10.0),
                                   "DeltaX", DoubleValue (0.0),
                                   "DeltaY", DoubleValue (0.0),
                                   "GridWidth", UintegerValue (1),
                                   "LayoutType", StringValue ("RowFirst"));
    mobility.Install (apNode);

    InternetStackHelper stack;
    stack.Install (apNode);
    stack.Install (staNodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign (apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    ApplicationContainer sourceApps;
    ApplicationContainer sinkApps;

    if (protocol == "Udp") {
        UdpEchoServerHelper echoServer (port);
        sinkApps = echoServer.Install (apNode.Get (0));
        sinkApps.Start (Seconds (1.0));
        sinkApps.Stop (Seconds (simulationTime + 1));

        UdpClientHelper echoClient (apInterface.GetAddress (0), port);
        echoClient.SetAttribute ("MaxPackets", UintegerValue (4294967295));
        echoClient.SetAttribute ("Interval", TimeValue (Time ("1us")));
        echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
        sourceApps = echoClient.Install (staNodes);
        sourceApps.Start (Seconds (2.0));
        sourceApps.Stop (Seconds (simulationTime));
    } else {
        PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
        sinkApps = sink.Install (apNode.Get (0));
        sinkApps.Start (Seconds (1.0));
        sinkApps.Stop (Seconds (simulationTime + 1));

        BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), port));
        source.SetAttribute ("MaxBytes", UintegerValue (0));
        sourceApps = source.Install (staNodes);
        sourceApps.Start (Seconds (2.0));
        sourceApps.Stop (Seconds (simulationTime));
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

    Simulator::Stop (Seconds (simulationTime + 2));

    Simulator::Run ();

    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
    double goodput = 0;
    for (auto i = stats.begin (); i != stats.end (); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
        if (t.destinationAddress == apInterface.GetAddress (0)) {
            goodput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1000000;
            std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
            std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
            std::cout << "  Throughput: " << goodput << " Mbps\n";

            NS_ASSERT_MSG (goodput > expectedThroughput * 0.8, "Obtained throughput is less than 80% of the expected one");
        }
    }

    Simulator::Destroy ();

    return 0;
}