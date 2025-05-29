#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTosSimulation");

int main(int argc, char* argv[]) {
    bool verbose = false;
    uint32_t nsta = 3;
    uint32_t htMcs = 7;
    uint32_t channelWidth = 20;
    bool shortGuardInterval = true;
    double distance = 5.0;
    bool rtsCts = false;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if set.", verbose);
    cmd.AddValue("nsta", "Number of wifi stations", nsta);
    cmd.AddValue("htMcs", "HT MCS value (0-7)", htMcs);
    cmd.AddValue("channelWidth", "Channel width (20 or 40 MHz)", channelWidth);
    cmd.AddValue("shortGuardInterval", "Use short guard interval (1=true, 0=false)", shortGuardInterval);
    cmd.AddValue("distance", "Distance between AP and stations", distance);
    cmd.AddValue("rtsCts", "Enable RTS/CTS (1=true, 0=false)", rtsCts);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("WifiTosSimulation", LOG_LEVEL_INFO);
    }

    if (htMcs > 7) {
        std::cerr << "HT MCS value must be between 0 and 7" << std::endl;
        return 1;
    }

    if (channelWidth != 20 && channelWidth != 40) {
        std::cerr << "Channel width must be 20 or 40 MHz" << std::endl;
        return 1;
    }

    Config::SetDefault("ns3::WifiMacQueue::MaxPackets", UintegerValue(1000));

    NodeContainer staNodes;
    staNodes.Create(nsta);
    NodeContainer apNode;
    apNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    // Configure STA
    mac.SetType("ns3::StaWifiMac",
        "Ssid", SsidValue(ssid),
        "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    // Configure AP
    mac.SetType("ns3::ApWifiMac",
        "Ssid", SsidValue(ssid),
        "BeaconInterval", TimeValue(Seconds(0.05)));

    NetDeviceContainer apDevices = wifi.Install(phy, mac, apNode);

    // Configure HT
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (channelWidth));
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/GuardInterval", BooleanValue (shortGuardInterval));
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");
    std::stringstream ss;
    ss << "HtMcs" << htMcs;
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue(ss.str()),
                                "ControlMode", StringValue(ss.str()));

    if (rtsCts) {
        Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/RtsCtsThreshold", UintegerValue (1));
    }

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
        "MinX", DoubleValue(0.0),
        "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(distance),
        "DeltaY", DoubleValue(0.0),
        "GridWidth", UintegerValue(nsta),
        "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNodes);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staNodeInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apNodeInterfaces = address.Assign(apDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP traffic
    uint16_t port = 9;
    UdpServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(apNode);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper echoClient(apNodeInterfaces.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(0xffffffff));
    echoClient.SetAttribute("Interval", TimeValue(MicroSeconds(1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1472));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < nsta; ++i) {
        clientApps.Add(echoClient.Install(staNodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));


    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(11.0));

    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    double total_throughput = 0.0;

    for (auto i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if (t.destinationAddress == apNodeInterfaces.GetAddress(0)) {
            double throughput = (i->second.rxBytes * 8.0) / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1000000;
            total_throughput += throughput;
            std::cout << "Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << " Throughput: " << throughput << " Mbps" << std::endl;
        }
    }

    std::cout << "Total throughput: " << total_throughput << " Mbps" << std::endl;

    Simulator::Destroy();

    return 0;
}