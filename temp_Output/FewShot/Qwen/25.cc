#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wi-FiMCSChannelWidthTest");

int main(int argc, char *argv[]) {
    // Simulation parameters with defaults
    uint32_t nStations = 1;
    uint32_t payloadSize = 1472;   // bytes
    uint32_t mcsValue = 7;
    uint32_t channelWidth = 20;    // MHz
    bool enableRtsCts = false;
    bool enableUlOfdma = true;
    bool enableDlMu = true;
    bool enableExtendedBlockAck = true;
    bool useShortGuardInterval = true;
    uint32_t band = 5;             // GHz (2.4, 5 or 6)
    std::string phyMode = "Yans";
    double distance = 5.0;         // meters
    Time simulationTime = Seconds(10);
    DataRate targetDataRate = DataRate("100Mbps");
    bool useTcp = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nStations", "Number of stations", nStations);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("mcsValue", "MCS value to use", mcsValue);
    cmd.AddValue("channelWidth", "Channel width in MHz", channelWidth);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
    cmd.AddValue("enableUlOfdma", "Enable UL OFDMA", enableUlOfdma);
    cmd.AddValue("enableDlMu", "Enable DL MU-PPDU", enableDlMu);
    cmd.AddValue("enableExtendedBlockAck", "Enable extended block ack", enableExtendedBlockAck);
    cmd.AddValue("useShortGuardInterval", "Use short guard interval", useShortGuardInterval);
    cmd.AddValue("band", "Wi-Fi band (2.4, 5, or 6 GHz)", band);
    cmd.AddValue("phyMode", "PHY mode: Yans or Spectrum", phyMode);
    cmd.AddValue("distance", "Distance between AP and stations (meters)", distance);
    cmd.AddValue("simulationTime", "Total simulation time", simulationTime);
    cmd.AddValue("targetDataRate", "Target data rate for application", targetDataRate);
    cmd.AddValue("useTcp", "Use TCP instead of UDP", useTcp);
    cmd.Parse(argc, argv);

    // Enable logging if needed
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("TcpSocketImpl", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer wifiApNode;
    wifiApNode.Create(1);
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nStations);

    // Create channels
    Ptr<MultiModelSpectrumChannel> spectrumChannel;
    if (phyMode == "Spectrum") {
        spectrumChannel = CreateObject<MultiModelSpectrumChannel>();
        Ptr<FriisPropagationLossModel> lossModel = CreateObject<FriisPropagationLossModel>();
        spectrumChannel->AddPropagationLossModel(lossModel);
        Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel>();
        spectrumChannel->SetPropagationDelayModel(delayModel);
    }

    YansWifiPhyHelper phy;
    WifiHelper wifi;
    WifiMacHelper mac;
    Ssid ssid = Ssid("wifi-test-mcs");

    // Set standard based on band and enable HE
    uint32_t freq = (band == 2) ? 2412 : ((band == 5) ? 5180 : 5955); // in MHz
    wifi.SetStandard(WIFI_STANDARD_80211ax_6GHZ);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                  "DataMode", StringValue("HeMcs" + std::to_string(mcsValue)),
                                  "ControlMode", StringValue("HeMcs" + std::to_string(mcsValue)));

    // PHY configuration
    if (phyMode == "Yans") {
        phy = YansWifiPhyHelper::Default();
    } else if (phyMode == "Spectrum") {
        phy = SpectrumWifiPhyHelper::Default();
        phy.SetChannel(spectrumChannel);
    }
    phy.Set("ChannelWidth", UintegerValue(channelWidth));
    phy.Set("ShortGuardIntervalSupported", BooleanValue(useShortGuardInterval));
    phy.Set("PreambleDetectionSupported", BooleanValue(true));

    // MAC configuration
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(MilliSeconds(100)),
                "EnableDownlinkMu", BooleanValue(enableDlMu),
                "EnableUplinkOfdma", BooleanValue(enableUlOfdma),
                "EnableRts", BooleanValue(enableRtsCts),
                "ExtendedBlockAck", BooleanValue(enableExtendedBlockAck));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0),
                                  "GridWidth", UintegerValue(nStations),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    // Internet stack
    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // Applications
    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    if (useTcp) {
        // TCP Server on AP
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
        serverApps = sinkHelper.Install(wifiApNode.Get(0));
        serverApps.Start(Seconds(0.0));
        serverApps.Stop(simulationTime);

        // TCP Clients on stations
        for (uint32_t i = 0; i < nStations; ++i) {
            BulkSendHelper clientHelper("ns3::TcpSocketFactory", InetSocketAddress(apInterface.GetAddress(0), 9));
            clientHelper.SetAttribute("MaxBytes", UintegerValue(0));
            clientHelper.SetAttribute("SendSize", UintegerValue(payloadSize));
            ApplicationContainer clientApp = clientHelper.Install(wifiStaNodes.Get(i));
            clientApp.Start(Seconds(1.0));
            clientApp.Stop(simulationTime);
        }
    } else {
        // UDP Echo Server on AP
        UdpEchoServerHelper echoServer(9);
        serverApps = echoServer.Install(wifiApNode.Get(0));
        serverApps.Start(Seconds(0.0));
        serverApps.Stop(simulationTime);

        // UDP Echo Client on each station
        for (uint32_t i = 0; i < nStations; ++i) {
            UdpEchoClientHelper client(staInterfaces.GetAddress(i), 9);
            client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
            client.SetAttribute("Interval", TimeValue(Seconds(0.001)));
            client.SetAttribute("PacketSize", UintegerValue(payloadSize));
            ApplicationContainer clientApp = client.Install(wifiStaNodes.Get(i));
            clientApp.Start(Seconds(1.0));
            clientApp.Stop(simulationTime);
        }
    }

    // Flow monitor setup
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(simulationTime);
    Simulator::Run();

    // Output results
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::cout << "\nThroughput Results:" << std::endl;
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        std::cout << "Flow ID: " << iter->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")" << std::endl;
        std::cout << "  Tx Packets: " << iter->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << iter->second.rxPackets << std::endl;
        std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1e6 << " Mbps" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}