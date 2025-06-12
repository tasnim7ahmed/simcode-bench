#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/packet-sink.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/on-off-application.h"
#include "ns3/on-off-helper.h"
#include "ns3/command-line.h"
#include "ns3/ssid.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiFixedRssExample");

int main(int argc, char* argv[]) {
    bool verbose = false;
    int rss = -80;  // dBm
    uint32_t packetSize = 1472; // bytes
    uint32_t numPackets = 1000;
    double interval = 0.001; // seconds
    std::string dataRate = "11Mbps";

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if set.", verbose);
    cmd.AddValue("rss", "Fixed RSS value (dBm).", rss);
    cmd.AddValue("packetSize", "Size of packet sent in bytes", packetSize);
    cmd.AddValue("numPackets", "Number of packets generated", numPackets);
    cmd.AddValue("interval", "Interval between packets in seconds", interval);
    cmd.AddValue("dataRate", "Data rate for OnOff application", dataRate);
    cmd.Parse(argc, argv);

    // Enable logging
    if (verbose) {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    // Create nodes: one AP and one station
    NodeContainer nodes;
    nodes.Create(2);
    NodeContainer apNode = NodeContainer(nodes.Get(0));
    NodeContainer staNode = NodeContainer(nodes.Get(1));

    // Set up Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());

    // Configure the RSS loss model
    wifiPhy.Set("TxPowerStart", DoubleValue(20));
    wifiPhy.Set("TxPowerEnd", DoubleValue(20));
    wifiPhy.Set("TxPowerLevels", UintegerValue(1));

    // Add a fixed RSS loss model
    Ptr<FixedRssLossModel> lossModel = CreateObject<FixedRssLossModel>();
    lossModel->SetRss(rss);
    wifiChannel.AddPropagationLoss("ns3::FixedRssLossModel", "Rss", DoubleValue(rss));

    // Configure MAC layer
    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns-3-ssid");

    // Setup AP
    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, apNode);

    // Setup STA
    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice = wifi.Install(wifiPhy, wifiMac, staNode);

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(nodes);

    // Install and start applications on STA (client) and AP (server)

    //AP (Server)
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(0), 9));
    ApplicationContainer sinkApp = sinkHelper.Install(apNode);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    //STA (Client)
    OnOffHelper onOffHelper("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(0), 9));
    onOffHelper.SetConstantRate(DataRate(dataRate));
    onOffHelper.SetPacketSize(packetSize);
    ApplicationContainer clientApp = onOffHelper.Install(staNode);
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // PCAP Tracing
    wifiPhy.EnablePcap("wifi-fixed-rss", apDevice);
    wifiPhy.EnablePcap("wifi-fixed-rss", staDevice);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}