#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiAdhocGridOlsrExample");

int main(int argc, char *argv[])
{
    uint32_t gridWidth = 5;
    uint32_t gridHeight = 5;
    double gridDistance = 30.0;
    std::string phyMode = "DsssRate11Mbps";
    uint32_t packetSize = 1000;
    uint32_t numPackets = 1;
    uint32_t sourceNodeId = 24;
    uint32_t sinkNodeId = 0;
    bool verbose = false;
    bool enablePcap = false;
    bool enableAscii = false;
    double rssiThreshold = -80.0; // dBm

    CommandLine cmd(__FILE__);
    cmd.AddValue("phyMode", "Physical layer mode (e.g. DsssRate11Mbps)", phyMode);
    cmd.AddValue("gridDistance", "Separation (in meters) between nodes", gridDistance);
    cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
    cmd.AddValue("numPackets", "Number of packets to send", numPackets);
    cmd.AddValue("sourceNode", "Source node index", sourceNodeId);
    cmd.AddValue("sinkNode", "Sink node index", sinkNodeId);
    cmd.AddValue("verbose", "Enable verbose mode", verbose);
    cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
    cmd.AddValue("enableAscii", "Enable ASCII tracing", enableAscii);
    cmd.AddValue("rssiThreshold", "RX signal minimum (dBm)", rssiThreshold);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("WifiAdhocGridOlsrExample", LOG_LEVEL_INFO);
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    uint32_t numNodes = gridWidth * gridHeight;
    NodeContainer nodes;
    nodes.Create(numNodes);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();

    // Propagation loss: Cutoff at some RX gain
    Ptr<RangePropagationLossModel> rangeModel = CreateObject<RangePropagationLossModel> ();
    double txPowerDbm = 20.0; // Default TX power for calculation
    // Calculate max range for this tx power and threshold
    double receiverThresholdDbm = rssiThreshold;
    double rxMaxRange = pow(10.0, (txPowerDbm - receiverThresholdDbm) / 20.0) * 1.0; // order of magnitude ~hundreds of m
    // But RangePropagationLossModel is strict distance cutoff
    // Instead, combine LogDistance and Range
    Ptr<LogDistancePropagationLossModel> logModel = CreateObject<LogDistancePropagationLossModel> ();
    Ptr<RangePropagationLossModel> rangeModel2 = CreateObject<RangePropagationLossModel> ();
    // Use a max distance so nodes only communicate when close enough
    double maxRange = 2.5 * gridDistance; // Empirically: for 30m spacing, allows some neighbors

    rangeModel2->SetAttribute("MaxRange", DoubleValue(maxRange));
    logModel->SetNext(rangeModel2);

    wifiChannel.SetPropagationLoss("ns3::LogDistancePropagationLossModel");
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    Ptr<YansWifiChannel> phyChannel = wifiChannel.Create();
    phyChannel->SetPropagationLossModel(logModel); // log + range

    wifiPhy.SetChannel(phyChannel);
    wifiPhy.Set("RxGain", DoubleValue(0));
    wifiPhy.Set("TxPowerStart", DoubleValue(txPowerDbm));
    wifiPhy.Set("TxPowerEnd", DoubleValue(txPowerDbm));
    wifiPhy.Set("EnergyDetectionThreshold", DoubleValue(-96.0)); // typical 802.11b thresholds
    wifiPhy.Set("CcaMode1Threshold", DoubleValue(-99.0));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue(phyMode),
                                "ControlMode", StringValue(phyMode));
    NqosWifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(gridDistance),
                                 "DeltaY", DoubleValue(gridDistance),
                                 "GridWidth", UintegerValue(gridWidth),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.Install(nodes);

    InternetStackHelper stack;
    OlsrHelper olsr;
    stack.SetRoutingHelper(olsr);
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.0.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP traffic: server on sink, client on source
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(sinkNodeId));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper client(interfaces.GetAddress(sinkNodeId), port);
    client.SetAttribute("MaxPackets", UintegerValue(numPackets));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps = client.Install(nodes.Get(sourceNodeId));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    if (enablePcap)
    {
        wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        wifiPhy.EnablePcap("wifi-adhoc-grid", devices, true);
    }

    AsciiTraceHelper ascii;
    if (enableAscii)
    {
        wifiPhy.EnableAsciiAll(ascii.CreateFileStream("wifi-adhoc-grid.tr"));
    }

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}