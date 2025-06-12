#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wi-Fi_80211ax_Configurable_Simulation");

int main(int argc, char *argv[])
{
    // Default simulation parameters
    uint32_t nStations = 5;
    double distance = 10.0; // meters
    uint32_t payloadSize = 1472; // bytes
    bool enableRtsCts = false;
    bool enableUlOfdma = true;
    bool enableDlMu = true;
    bool enableExtendedBlockAck = true;
    bool useTcp = false;
    std::string phyBand = "5GHz";
    std::string channelWidth = "20";
    std::string guardInterval = "800ns";
    std::string pcapFile = "wifi-80211ax-sim.pcap";
    double simulationTime = 10.0; // seconds

    CommandLine cmd(__FILE__);
    cmd.AddValue("nStations", "Number of stations", nStations);
    cmd.AddValue("distance", "Distance between AP and stations (m)", distance);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
    cmd.AddValue("enableUlOfdma", "Enable UL OFDMA", enableUlOfdma);
    cmd.AddValue("enableDlMu", "Enable DL MU PPDUs", enableDlMu);
    cmd.AddValue("enableExtendedBlockAck", "Enable extended block ack", enableExtendedBlockAck);
    cmd.AddValue("useTcp", "Use TCP instead of UDP", useTcp);
    cmd.AddValue("phyBand", "PHY band: 2.4, 5 or 6 GHz", phyBand);
    cmd.AddValue("channelWidth", "Channel width (MHz): 20, 40, 80, 160", channelWidth);
    cmd.AddValue("guardInterval", "Guard interval: 800ns, 1600ns, 3200ns", guardInterval);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    // Set default log level
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Setup nodes
    NodeContainer wifiApNode;
    wifiApNode.Create(1);
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nStations);

    // Setup mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0),
                                  "GridWidth", UintegerValue(10),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNodes);

    // Setup Wifi
    WifiMacHelper wifiMac;
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211ax);

    // Set PHY Band
    if (phyBand == "2.4GHz")
        wifiHelper.SetPhyBand(WIFI_PHY_BAND_2_4GHZ);
    else if (phyBand == "5GHz")
        wifiHelper.SetPhyBand(WIFI_PHY_BAND_5GHZ);
    else if (phyBand == "6GHz")
        wifiHelper.SetPhyBand(WIFI_PHY_BAND_6GHZ);

    // Set Channel Width
    uint16_t cw = std::stoi(channelWidth);
    wifiHelper.SetChannelWidth(cw);

    // Set Guard Interval
    uint16_t gi = 800;
    if (guardInterval == "1600ns")
        gi = 1600;
    else if (guardInterval == "3200ns")
        gi = 3200;
    wifiHelper.ConfigureHeOptions(gi);

    // Enable RTS/CTS
    if (enableRtsCts)
        wifiHelper.EnableRtsCts();

    // Use Spectrum or Yans
    Ssid ssid = Ssid("ns3-wifi-80211ax");
    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid),
                    "EnableDownlinkMu", BooleanValue(enableDlMu),
                    "EnableUplinkOfdma", BooleanValue(enableUlOfdma));
    NetDeviceContainer apDevice;
    apDevice = wifiHelper.Install(wifiApNode.Get(0));

    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices = wifiHelper.Install(wifiStaNodes, wifiApNode.Get(0));

    // Install Internet stack
    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);

    // Routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Applications
    uint16_t port = 9;
    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    if (useTcp)
    {
        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        serverApps = sink.Install(wifiStaNodes);

        OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(staInterfaces.GetAddress(0), port));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onoff.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
        onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
        clientApps = onoff.Install(wifiApNode);
    }
    else
    {
        UdpEchoServerHelper echoServer(port);
        serverApps = echoServer.Install(wifiStaNodes);

        UdpEchoClientHelper echoClient(staInterfaces.GetAddress(0), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.001)));
        echoClient.SetAttribute("PacketSize", UintegerValue(payloadSize));
        clientApps = echoClient.Install(wifiApNode);
    }

    serverApps.Start(Seconds(1.0));
    clientApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime + 0.1));
    clientApps.Stop(Seconds(simulationTime + 0.1));

    // Flow monitor for performance analysis
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // PCAP tracing
    wifiHelper.EnablePcap(pcapFile, apDevice.Get(0));

    Simulator::Stop(Seconds(simulationTime + 1.0));
    Simulator::Run();
    monitor->CheckForLostPackets();

    // Output results
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowStats> stats = monitor->GetFlowStats();

    double totalThroughput = 0;
    for (auto &[flowId, flowStats] : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowId);
        if (t.destinationPort == port)
        {
            std::cout << "Flow " << flowId << " (" << t.sourceAddress << " -> " << t.destinationAddress << "):\n";
            std::cout << "  Tx Packets: " << flowStats.txPackets << "\n";
            std::cout << "  Rx Packets: " << flowStats.rxPackets << "\n";
            std::cout << "  Throughput: " << flowStats.rxBytes * 8.0 / (flowStats.timeLastRxPacket.GetSeconds() - flowStats.timeFirstTxPacket.GetSeconds()) / 1e6 << " Mbps\n";
            totalThroughput += flowStats.rxBytes * 8.0 / simulationTime / 1e6;
        }
    }

    std::cout << "Total Throughput: " << totalThroughput << " Mbps" << std::endl;

    Simulator::Destroy();
    return 0;
}