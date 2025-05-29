#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiInterferenceSimulation");

int main(int argc, char* argv[]) {
    // Enable logging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("WifiInterferenceSimulation", LOG_LEVEL_INFO);

    // Set default values for simulation parameters
    double simulationTime = 10.0;
    double distance = 10.0;
    uint32_t mcsIndex = 7; // Example MCS index
    uint32_t channelWidthIndex = 0; // Example channel width index
    uint32_t guardIntervalIndex = 0; // Example guard interval index
    double waveformPower = 0.0; // dBm
    std::string wifiType = "ns3::Wifi80211n";
    std::string errorModelType = "ns3::RateErrorModel";
    double errorRate = 0.00001;
    bool enablePcap = false;

    // Command line arguments parsing
    CommandLine cmd;
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("distance", "Distance between nodes", distance);
    cmd.AddValue("mcsIndex", "MCS index", mcsIndex);
    cmd.AddValue("channelWidthIndex", "Channel width index", channelWidthIndex);
    cmd.AddValue("guardIntervalIndex", "Guard interval index", guardIntervalIndex);
    cmd.AddValue("waveformPower", "Waveform power in dBm", waveformPower);
    cmd.AddValue("wifiType", "Wifi standard type (e.g., ns3::Wifi80211n)", wifiType);
    cmd.AddValue("errorModelType", "Error Model Type", errorModelType);
    cmd.AddValue("errorRate", "Error Rate", errorRate);
    cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
    cmd.Parse(argc, argv);

    // Create nodes: STA (Station) and AP (Access Point)
    NodeContainer wifiNodes;
    wifiNodes.Create(2);
    NodeContainer nonWifiNode;
    nonWifiNode.Create(1);

    Ptr<Node> staNode = wifiNodes.Get(0);
    Ptr<Node> apNode = wifiNodes.Get(1);
    Ptr<Node> interferenceNode = nonWifiNode.Get(0);

    // Set up mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    // Interference node mobility
    MobilityHelper interferenceMobility;
    interferenceMobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(distance/2.0),
                                  "MinY", DoubleValue(distance/2.0),
                                  "DeltaX", DoubleValue(0.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));
    interferenceMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    interferenceMobility.Install(nonWifiNode);
    

    // Set up Wifi PHY and channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Wifi configuration
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n); // Use 802.11n by default
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNode);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(0.1)),
                "QosSupported", BooleanValue(true));

    NetDeviceContainer apDevices = wifi.Install(phy, mac, apNode);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer::Create(staDevices, apDevices));

    Ipv4Address staAddress = interfaces.GetAddress(0);
    Ipv4Address apAddress = interfaces.GetAddress(1);

    // Create UDP application
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(apNode);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime - 1.0));

    UdpEchoClientHelper echoClient(apAddress, 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(staNode);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime - 2.0));

    // Create a constant energy source on the interference node
    OutputStreamWrapper streamWrapper ("interference_trace.txt", std::ios::out);
    Names::Add ("/Names/InterferenceNode", interferenceNode);

    Simulator::Schedule(Seconds(1.0), [&, interferenceNode, streamWrapper] () mutable
    {
        Ptr<OutputStreamWrapper> ptr = CopyObject<OutputStreamWrapper> (streamWrapper);
        *ptr->GetStream () << Simulator::Now ().GetSeconds () << " " << waveformPower << std::endl;

        //This creates a constant wave on the spectrum.
        Simulator::Schedule(Seconds(0.01), [&, interferenceNode, streamWrapper]() mutable
        {
          SpectrumWifiPhyHelper::ApplyWaveform(interferenceNode, waveformPower);
        });
    });

    // Install FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Enable PCAP tracing
    if (enablePcap) {
        phy.EnablePcapAll("wifi-interference");
    }

    // Run the simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Print FlowMonitor statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << " Src Port " << t.sourcePort << " Dst Port " << t.destinationPort << "\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
    }

    Simulator::Destroy();
    return 0;
}