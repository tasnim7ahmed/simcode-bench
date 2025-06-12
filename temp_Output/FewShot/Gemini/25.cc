#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config-store-module.h"
#include "ns3/command-line.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

    bool verbose = false;
    uint32_t nSta = 1;
    std::string phyMode = "HtMcs7";
    double distance = 10.0;
    uint32_t packetSize = 1472;
    std::string dataRate = "50Mbps";
    std::string tcpVariant = "TcpNewReno";
    bool pcapTracing = false;
    bool enableRtsCts = false;
    bool use80plus80 = false;
    bool enableExtBlockAck = false;
    bool enableUlOfdma = false;
    std::string channelWidth = "20MHz";
    std::string guardInterval = "0.8us";
    uint32_t simulationTime = 10;
    bool useSpectrumPhy = false;
    double targetThroughputMbps = 50.0;
    std::string frequencyBand = "5GHz";

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if set.", verbose);
    cmd.AddValue("nSta", "Number of wifi stations", nSta);
    cmd.AddValue("phyMode", "Wifi phy mode", phyMode);
    cmd.AddValue("distance", "Distance between AP and stations", distance);
    cmd.AddValue("packetSize", "Size of packets generated", packetSize);
    cmd.AddValue("dataRate", "Application data rate", dataRate);
    cmd.AddValue("tcpVariant", "Transport protocol to use: TcpNewReno, "
                   "TcpIllinois, TcpWestwood, TcpWestwoodPlus, TcpHybla "
                   "TcpHighSpeed, TcpHtcp, TcpVegas, TcpScalable, TcpLp "
                   "or TcpYeah", tcpVariant);
    cmd.AddValue("pcap", "Enable pcap tracing", pcapTracing);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
    cmd.AddValue("use80plus80", "Use 80+80 MHz channel", use80plus80);
    cmd.AddValue("enableExtBlockAck", "Enable extended block ack", enableExtBlockAck);
    cmd.AddValue("enableUlOfdma", "Enable Uplink OFDMA", enableUlOfdma);
    cmd.AddValue("channelWidth", "Channel width (20MHz, 40MHz, 80MHz, 160MHz)", channelWidth);
    cmd.AddValue("guardInterval", "Guard interval (0.4us, 0.8us, 1.6us, 3.2us)", guardInterval);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("useSpectrumPhy", "Use Spectrum Phy model", useSpectrumPhy);
    cmd.AddValue("targetThroughput", "Target throughput in Mbps", targetThroughputMbps);
    cmd.AddValue("frequencyBand", "Frequency band (2.4GHz, 5GHz, 6GHz)", frequencyBand);

    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    tcpVariant = std::string("ns3::") + tcpVariant;
    TypeId tcpTid;
    tcpTid = TypeId::LookupByName(tcpVariant);

    GlobalValue::Bind("Tcp::Default", TypeIdValue(tcpTid));

    NodeContainer staNodes;
    staNodes.Create(nSta);
    NodeContainer apNode;
    apNode.Create(1);

    YansWifiChannelHelper channel;
    YansWifiPhyHelper phy;

    if (useSpectrumPhy) {
        channel = YansWifiChannelHelper::Default();
        channel.AddPropagationLoss("ns3::FriisPropagationLossModel");
        phy = YansWifiPhyHelper::Default();
        phy.Set("ChannelWidth", StringValue(channelWidth));

        SpectrumWifiPhyHelper spectrumPhy = SpectrumWifiPhyHelper::Default();
        spectrumPhy.SetChannel(channel.Create());
        phy = spectrumPhy;
    } else {
        channel = YansWifiChannelHelper::Default();
        channel.AddPropagationLoss("ns3::FriisPropagationLossModel");
        phy = YansWifiPhyHelper::Default();
        phy.Set("ChannelWidth", StringValue(channelWidth));
        phy.SetChannel(channel.Create());
    }

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211ax);

    WifiMacHelper mac;

    Ssid ssid = Ssid("ns-3-ssid");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, staNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconGeneration", BooleanValue(true),
                "BeaconInterval", TimeValue(Seconds(0.1)));

    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, apNode);

    MobilityHelper mobility;

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(nSta),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNodes);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);

    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staNodeIface;
    staNodeIface = address.Assign(staDevices);
    Ipv4InterfaceContainer apNodeIface;
    apNodeIface = address.Assign(apDevices);

    uint16_t port = 5000;

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(staNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime + 1));

    UdpClientHelper client(staNodeIface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295));
    client.SetAttribute("Interval", TimeValue(DataRate(dataRate) / packetSize));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = client.Install(apNode.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime + 1));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 1));

    if (pcapTracing) {
        phy.EnablePcap("wifi-80211ax", apDevices.Get(0));
    }

    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << " Src Port " << t.sourcePort << " Dst Port " << t.destinationPort << "\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
    }

    Simulator::Destroy();

    return 0;
}