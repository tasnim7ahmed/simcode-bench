#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiGoodput");

int main(int argc, char *argv[]) {
    bool verbose = false;
    uint32_t nWifi = 1;
    double simulationTime = 10;
    std::string dataRate = "50Mbps";
    std::string tcpVariant = "TcpNewReno";
    double distance = 5.0;
    bool rtsCtsEnabled = false;
    uint32_t packetSize = 1472;
    uint32_t port = 9;
    uint32_t mcs = 0;
    std::string channelWidth = "20";
    std::string guardInterval = "Long";

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if true", verbose);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("dataRate", "Data rate for the UDP client", dataRate);
    cmd.AddValue("tcpVariant", "Transport protocol to use: TcpNewReno, "
                   "TcpHybla, TcpHighSpeed, TcpHtcp, TcpVegas, TcpScalable, TcpLp, "
                   "TcpCubic, TcpWestwood, TcpWestwoodPlus", tcpVariant);
    cmd.AddValue("distance", "Distance between access point and station", distance);
    cmd.AddValue("rtsCtsEnabled", "Enable RTS/CTS", rtsCtsEnabled);
    cmd.AddValue("packetSize", "Size of UDP packets", packetSize);
    cmd.AddValue("mcs", "MCS value (0-7)", mcs);
    cmd.AddValue("channelWidth", "Channel Width (20 or 40)", channelWidth);
    cmd.AddValue("guardInterval", "Guard Interval (Short or Long)", guardInterval);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    tcpVariant = std::string("ns3::") + tcpVariant;
    TypeId tid = TypeId::LookupByName(tcpVariant);
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(tid));

    NodeContainer apContainer;
    apContainer.Create(1);

    NodeContainer staContainer;
    staContainer.Create(nWifi);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

    std::string ssid = "wifi-default";
    Ssid ssidObject = Ssid(ssid);

    WifiMacHelper mac;

    NetDeviceContainer apDevice;
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssidObject));

    apDevice = wifi.Install(phy, mac, apContainer);

    NetDeviceContainer staDevice;
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssidObject),
                "ActiveProbing", BooleanValue(false));

    staDevice = wifi.Install(phy, mac, staContainer);

    if (rtsCtsEnabled) {
        Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/RtsCtsThreshold", StringValue ("0"));
    }

    Wifi80211nHelper wifi80211n;
    wifi80211n.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                        "DataMode", StringValue("OfdmRate" + std::to_string(mcs) + "MbpsHT-" + channelWidth + "MHz"),
                                        "ControlMode", StringValue("OfdmRate" + std::to_string(mcs) + "MbpsHT-" + channelWidth + "MHz"));

    MobilityHelper mobility;

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(apContainer);

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(distance),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.Install(staContainer);

    InternetStackHelper stack;
    stack.Install(apContainer);
    stack.Install(staContainer);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(apContainer.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime + 1));

    UdpClientHelper client(apInterface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295));
    client.SetAttribute("Interval", TimeValue(DataRate(dataRate) / (packetSize * 8)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = client.Install(staContainer.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime + 1));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 1));

    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double goodput = 0.0;
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if (t.destinationAddress == apInterface.GetAddress(0)) {
            goodput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
            std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
            std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
            std::cout << "  Throughput: " << goodput << " Mbps\n";
        }
    }

    monitor->SerializeToXmlFile("wifi_goodput.xml", false, true);

    Simulator::Destroy();

    return 0;
}