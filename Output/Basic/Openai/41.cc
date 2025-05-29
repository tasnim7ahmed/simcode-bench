#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/applications-module.h"
#include "ns3/pcap-file-wrapper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocWifiGrid");

int main(int argc, char *argv[])
{
    uint32_t gridWidth = 5;
    uint32_t gridHeight = 5;
    double gridStep = 30.0;
    uint32_t packetSize = 1000;
    uint32_t numPackets = 1;
    uint32_t sourceNode = 24;
    uint32_t sinkNode = 0;
    std::string phyMode = "DsssRate11Mbps";
    bool verbose = false;
    bool enablePcap = false;
    bool enableAscii = false;
    double simTime = 10.0;

    CommandLine cmd;
    cmd.AddValue("phyMode", "Wifi PHY mode (default: DsssRate11Mbps)", phyMode);
    cmd.AddValue("gridDistance", "Separation between grid nodes (default: 30.0)", gridStep);
    cmd.AddValue("packetSize", "Application packet size in bytes (default: 1000)", packetSize);
    cmd.AddValue("numPackets", "Number of packets to send (default: 1)", numPackets);
    cmd.AddValue("sourceNode", "Index of source node (default: 24)", sourceNode);
    cmd.AddValue("sinkNode", "Index of sink node (default: 0)", sinkNode);
    cmd.AddValue("verbose", "Enable verbose logging (default: false)", verbose);
    cmd.AddValue("enablePcap", "Enable PCAP tracing (default: false)", enablePcap);
    cmd.AddValue("enableAscii", "Enable ASCII tracing (default: false)", enableAscii);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("AdhocWifiGrid", LOG_LEVEL_INFO);
        LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
        LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    }

    uint32_t nNodes = gridWidth * gridHeight;
    NodeContainer nodes;
    nodes.Create(nNodes);

    // Mobility - GridPositionAllocator
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(gridStep),
                                 "DeltaY", DoubleValue(gridStep),
                                 "GridWidth", UintegerValue(gridWidth),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.Install(nodes);

    // Wifi PHY and MAC configuration
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue(phyMode),
                                 "ControlMode", StringValue(phyMode), 
                                 "NonUnicastMode", StringValue(phyMode));

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.Set("RxGain", DoubleValue(0));
    wifiPhy.Set("TxPowerStart", DoubleValue(20.0));
    wifiPhy.Set("TxPowerEnd", DoubleValue(20.0));

    // Set propagation
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(gridStep * 2.5));
    wifiChannel.AddPropagationLoss("ns3::LogDistancePropagationLossModel",
                                   "ReferenceDistance", DoubleValue(1.0),
                                   "Exponent", DoubleValue(3.0),
                                   "ReferenceLoss", DoubleValue(46.6777)); // 1m, freq 2.4GHz, 20dBm

    Ptr<YansWifiChannel> channel = wifiChannel.Create();
    wifiPhy.SetChannel(channel);

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Install Internet stack with OLSR
    OlsrHelper olsr;
    InternetStackHelper stack;
    stack.SetRoutingHelper(olsr);
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install PacketSink on sink node
    uint16_t port = 5000;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(sinkNode), port));
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sinkHelper.Install(nodes.Get(sinkNode));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simTime));

    // Install OnOffApplication on source node
    OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
    onoff.SetAttribute("DataRate", StringValue("54Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff.SetAttribute("MaxBytes", UintegerValue(packetSize * numPackets));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(simTime - 1.0)));

    ApplicationContainer sourceApp = onoff.Install(nodes.Get(sourceNode));

    // Tracing
    AsciiTraceHelper ascii;
    if (enablePcap)
    {
        wifiPhy.EnablePcapAll("adhoc-grid");
    }
    if (enableAscii)
    {
        wifiPhy.EnableAsciiAll(ascii.CreateFileStream("adhoc-grid.tr"));
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}