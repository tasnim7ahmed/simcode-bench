#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/command-line.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool verbose = false;
    uint32_t packetSize = 1472;
    std::string dataRate = "50Mbps";
    std::string phyRate = "HtMcs7";
    double simulationTime = 10;
    double distance = 5.0;
    bool rtsCtsEnabled = false;
    std::string transportProtocol = "Udp";
    double expectedThroughput = 50;
    int mcs = 7;
    int channelWidth = 20;
    std::string guardInterval = "Long";

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell applications to log if true", verbose);
    cmd.AddValue("packetSize", "Size of packet sent in bytes", packetSize);
    cmd.AddValue("dataRate", "Data rate for UDP traffic", dataRate);
    cmd.AddValue("phyRate", "Physical layer rate for Wi-Fi", phyRate);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("distance", "Distance between STA and AP in meters", distance);
    cmd.AddValue("rtsCts", "Enable RTS/CTS", rtsCtsEnabled);
    cmd.AddValue("transportProtocol", "Transport protocol (Udp or Tcp)", transportProtocol);
    cmd.AddValue("expectedThroughput", "Expected throughput in Mbps", expectedThroughput);
    cmd.AddValue("mcs", "MCS value (0-7)", mcs);
    cmd.AddValue("channelWidth", "Channel width (20 or 40)", channelWidth);
    cmd.AddValue("guardInterval", "Guard interval (Short or Long)", guardInterval);

    cmd.Parse(argc, argv);

    if (transportProtocol != "Udp" && transportProtocol != "Tcp") {
        std::cerr << "Invalid transport protocol. Must be Udp or Tcp." << std::endl;
        return 1;
    }

    if (mcs < 0 || mcs > 7) {
        std::cerr << "Invalid MCS value. Must be between 0 and 7." << std::endl;
        return 1;
    }

    if (channelWidth != 20 && channelWidth != 40) {
        std::cerr << "Invalid channel width. Must be 20 or 40." << std::endl;
        return 1;
    }

    if (guardInterval != "Short" && guardInterval != "Long") {
        std::cerr << "Invalid guard interval. Must be Short or Long." << std::endl;
        return 1;
    }

    NodeContainer staNodes;
    staNodes.Create(1);
    NodeContainer apNodes;
    apNodes.Create(1);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns-3-ssid");
    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, staNodes);

    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid),
                    "BeaconGeneration", BooleanValue(true),
                    "BeaconInterval", TimeValue(Seconds(0.1)));

    NetDeviceContainer apDevices = wifi.Install(wifiPhy, wifiMac, apNodes);

    if (rtsCtsEnabled) {
        wifi.SetRemoteStationManager("ns3::RtsCtsWifiManager");
    }
    else{
          wifi.SetRemoteStationManager ("ns3::IdealWifiManager");
    }

    std::stringstream ss;
    ss << "HtMcs" << mcs;
    phyRate = ss.str();
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                  "DataMode",StringValue (phyRate),
                                  "ControlMode",StringValue (phyRate));

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNodes);
    mobility.Install(apNodes);

    InternetStackHelper internet;
    internet.Install(apNodes);
    internet.Install(staNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i = ipv4.Assign(staDevices);
    ipv4.Assign(apDevices);
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 50000;
    Address sinkAddress(Ipv4Address::GetAny());
    ApplicationContainer sinkApps;

    if (transportProtocol == "Udp") {
        UdpServerHelper echoServerHelper(port);
        sinkApps = echoServerHelper.Install(apNodes);
        sinkApps.Start(Seconds(0.0));
        sinkApps.Stop(Seconds(simulationTime + 1));
        UdpClientHelper echoClientHelper(i.GetAddress(0), port);
        echoClientHelper.SetAttribute("MaxPackets", UintegerValue(4294967295));
        echoClientHelper.SetAttribute("Interval", TimeValue(Seconds(0.00002))); // Adjust as needed
        echoClientHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
        sinkApps = echoClientHelper.Install(staNodes);
    } else {
        TcpServerHelper echoServerHelper(port);
        sinkApps = echoServerHelper.Install(apNodes);
        sinkApps.Start(Seconds(0.0));
        sinkApps.Stop(Seconds(simulationTime + 1));

        TcpClientHelper echoClientHelper(i.GetAddress(0), port);
        echoClientHelper.SetAttribute("MaxPackets", UintegerValue(4294967295));
        echoClientHelper.SetAttribute("Interval", TimeValue(Seconds(0.00002))); // Adjust as needed
        echoClientHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
        sinkApps = echoClientHelper.Install(staNodes);

    }

    sinkApps.Start(Seconds(1.0));
    sinkApps.Stop(Seconds(simulationTime + 1));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 1));

    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double totalGoodput = 0;
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if (t.destinationAddress == i.GetAddress(0)) {
             double goodput = (i->second.rxBytes * 8.0) / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1000000;
            totalGoodput+=goodput;
             std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
             std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
             std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
             std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
             std::cout << "  Throughput: " << goodput << " Mbps\n";
        }
    }

    std::cout << "Total Goodput: " << totalGoodput << " Mbps" << std::endl;

    Simulator::Destroy();

    return 0;
}