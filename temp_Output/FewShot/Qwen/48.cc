#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/pcap-file-wrapper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiPhySimulation");

int main(int argc, char *argv[]) {
    std::string phyType = "YansWifiPhy";
    double simulationTime = 10.0;
    double distance = 10.0; // meters
    uint8_t mcsValue = 3;
    uint16_t channelWidth = 20;
    bool useShortGuardInterval = false;
    bool useUdp = true;
    bool pcapEnabled = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("phyType", "PHY type: YansWifiPhy or SpectrumWifiPhy", phyType);
    cmd.AddValue("simulationTime", "Total simulation time (seconds)", simulationTime);
    cmd.AddValue("distance", "Distance between nodes (meters)", distance);
    cmd.AddValue("mcs", "MCS index value", mcsValue);
    cmd.AddValue("channelWidth", "Channel width in MHz (20, 40, 80, etc.)", channelWidth);
    cmd.AddValue("shortGuardInterval", "Use short guard interval (true/false)", useShortGuardInterval);
    cmd.AddValue("useUdp", "Use UDP instead of TCP", useUdp);
    cmd.AddValue("pcap", "Enable PCAP capture", pcapEnabled);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Set up mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));       // AP
    positionAlloc->Add(Vector(distance, 0.0, 0.0));   // STA
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(wifiStaNode);
    mobility.Install(wifiApNode);

    // Install Wi-Fi
    WifiMacHelper wifiMac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5MHZ);

    // Configure PHY
    if (phyType == "YansWifiPhy") {
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        wifiPhy.SetErrorRateModel("ns3::NistErrorRateModel");
        wifi.Install(wifiPhy, wifiMac, wifiStaNode);
        wifi.Install(wifiPhy, wifiMac, wifiApNode);
    } else if (phyType == "SpectrumWifiPhy") {
        SpectrumWifiPhyHelper wifiPhy = SpectrumWifiPhyHelper::Default();
        wifiPhy.SetErrorRateModel("ns3::NistErrorRateModel");
        wifi.Install(wifiPhy, wifiMac, wifiStaNode);
        wifi.Install(wifiPhy, wifiMac, wifiApNode);
    }

    // Set MCS and channel parameters
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/Mcs", UintegerValue(mcsValue));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/ChannelWidth", UintegerValue(channelWidth));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/ShortGuardIntervalSupported", BooleanValue(useShortGuardInterval));

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNode);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterface = address.Assign(wifiStaNode.Get(0)->GetDevice(0));
    Ipv4InterfaceContainer apInterface = address.Assign(wifiApNode.Get(0)->GetDevice(0));

    // Traffic configuration
    ApplicationContainer serverApp;
    ApplicationContainer clientApp;

    if (useUdp) {
        UdpEchoServerHelper echoServer(9);
        serverApp = echoServer.Install(wifiStaNode.Get(0));
        serverApp.Start(Seconds(0.0));
        serverApp.Stop(Seconds(simulationTime));

        UdpEchoClientHelper echoClient(staInterface.GetAddress(0), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(1000000));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.001)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        clientApp = echoClient.Install(wifiApNode.Get(0));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(simulationTime));
    } else {
        uint16_t port = 50000;
        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        serverApp = sink.Install(wifiStaNode.Get(0));
        serverApp.Start(Seconds(0.0));
        serverApp.Stop(Seconds(simulationTime));

        OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(staInterface.GetAddress(0), port));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onoff.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
        onoff.SetAttribute("PacketSize", UintegerValue(1024));

        clientApp = onoff.Install(wifiApNode.Get(0));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(simulationTime));
    }

    // Enable logging for signal and noise
    LogComponentEnable("YansWifiPhy", LOG_LEVEL_DEBUG);
    LogComponentEnable("SpectrumWifiPhy", LOG_LEVEL_DEBUG);

    // Pcap setup
    if (pcapEnabled) {
        AsciiTraceHelper asciiTraceHelper;
        wifiPhy.EnableAsciiAll(asciiTraceHelper.CreateFileStream("wifi-phy-simulation.tr"));
        wifiPhy.EnablePcapAll("wifi-phy-simulation");
    }

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Report results
    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = static_cast<Ipv4FlowClassifier*>(monitor->GetClassifier().Peek())->FindFlow(iter->first);
        std::cout << "\nFlow ID: " << iter->first << " Src Addr: " << t.sourceAddress << " Dst Addr: " << t.destinationAddress << std::endl;
        std::cout << "Throughput: " << iter->second.rxBytes * 8.0 / simulationTime / 1000 / 1000 << " Mbps" << std::endl;
        std::cout << "Received Packets: " << iter->second.rxPackets << std::endl;
        std::cout << "Dropped Packets: " << iter->second.packetsDropped.size() << std::endl;
        std::cout << "Signal Strength: TODO (Requires custom tracing)" << std::endl;
        std::cout << "Noise: TODO (Requires custom tracing)" << std::endl;
        std::cout << "SNR: TODO (Requires custom tracing)" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}