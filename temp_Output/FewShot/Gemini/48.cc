#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/propagation-loss-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wifi80211nSimulation");

int main(int argc, char *argv[]) {
    bool verbose = false;
    uint32_t nWifi = 2;
    double simulationTime = 10.0;
    double distance = 10.0;
    std::string phyMode = "OfdmRate6Mbps";
    uint32_t mcs = 7;
    bool enablePcap = false;
    std::string protocol = "UDP"; // Can be "UDP" or "TCP"
    bool useSpectrumPhy = false;
    int channelWidth = 20; // 20 or 40 MHz
    std::string guardInterval = "800ns"; // "800ns" or "400ns"

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if possible", verbose);
    cmd.AddValue("numNodes", "Number of wifi STA devices", nWifi);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("distance", "Distance between nodes in meters", distance);
    cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
    cmd.AddValue("mcs", "MCS index", mcs);
    cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
    cmd.AddValue("protocol", "Transport protocol (UDP or TCP)", protocol);
    cmd.AddValue("useSpectrumPhy", "Use SpectrumWifiPhy instead of YansWifiPhy", useSpectrumPhy);
    cmd.AddValue("channelWidth", "Channel width (20 or 40 MHz)", channelWidth);
    cmd.AddValue("guardInterval", "Guard interval (800ns or 400ns)", guardInterval);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("Wifi80211nSimulation", LOG_LEVEL_INFO);
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
        LogComponentEnable("TcpClient", LOG_LEVEL_INFO);
        LogComponentEnable("TcpServer", LOG_LEVEL_INFO);
    }

    NodeContainer wifiNodes;
    wifiNodes.Create(nWifi);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    if (useSpectrumPhy) {
        wifiPhy = SpectrumWifiPhyHelper::Default();
    }

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiChannel.SetPropagationDelay("Model", StringValue("ns3::ConstantSpeedPropagationDelayModel"));
    wifiChannel.AddPropagationLoss("Model", StringValue("ns3::FriisPropagationLossModel"));

    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns-3-ssid");
    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(wifiPhy, wifiMac, wifiNodes.Get(0));
    for (uint32_t i = 1; i < nWifi; ++i) {
        staDevices.Add(wifi.Install(wifiPhy, wifiMac, wifiNodes.Get(i)));
    }

    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid),
                    "BeaconInterval", TimeValue(MilliSeconds(100)));

    NetDeviceContainer apDevices;
    apDevices = wifi.Install(wifiPhy, wifiMac, wifiNodes.Get(0));

    if (enablePcap) {
        wifiPhy.EnablePcapAll("wifi-80211n");
    }

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(nWifi),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    InternetStackHelper internet;
    internet.Install(wifiNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i = ipv4.Assign(NetDeviceContainer::Create(apDevices, staDevices));

    uint16_t port = 50000;
    ApplicationContainer serverApp;
    ApplicationContainer clientApp;

    if (protocol == "UDP") {
        UdpServerHelper server(port);
        serverApp = server.Install(wifiNodes.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(simulationTime));

        UdpClientHelper client(i.GetAddress(0), port);
        client.SetAttribute("MaxPackets", UintegerValue(4294967295));
        client.SetAttribute("Interval", TimeValue(MicroSeconds(10)));
        client.SetAttribute("PacketSize", UintegerValue(1024));
        clientApp = client.Install(wifiNodes.Get(1));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(simulationTime));

    } else if (protocol == "TCP") {
        TcpServerHelper server(port);
        serverApp = server.Install(wifiNodes.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(simulationTime));

        TcpClientHelper client(i.GetAddress(0), port);
        client.SetAttribute("MaxPackets", UintegerValue(4294967295));
        client.SetAttribute("Interval", TimeValue(MicroSeconds(10)));
        client.SetAttribute("PacketSize", UintegerValue(1024));
        clientApp = client.Install(wifiNodes.Get(1));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(simulationTime));
    } else {
        std::cerr << "Invalid protocol. Use UDP or TCP." << std::endl;
        return 1;
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
    }

    Simulator::Destroy();
    return 0;
}