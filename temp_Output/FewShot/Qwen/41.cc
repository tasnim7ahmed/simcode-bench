#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/olsr-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/drr-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessGridSimulation");

int main(int argc, char *argv[]) {
    std::string phyMode("DsssRate1Mbps");
    double distance = 100;  // in meters
    uint32_t packetSize = 1000;
    uint32_t numPackets = 1;
    uint32_t sourceNode = 24;
    uint32_t sinkNode = 0;
    bool verbose = false;
    bool tracing = false;
    std::string phyLayer = "DsssRate1Mbps";

    CommandLine cmd(__FILE__);
    cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
    cmd.AddValue("distance", "distance in meters between grid nodes", distance);
    cmd.AddValue("packetSize", "size of application packet sent", packetSize);
    cmd.AddValue("numPackets", "number of packets to send", numPackets);
    cmd.AddValue("sourceNode", "Index of the source node", sourceNode);
    cmd.AddValue("sinkNode", "Index of the sink node", sinkNode);
    cmd.AddValue("verbose", "turn on all Wifi logging", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.AddValue("phyLayer", "Phy layer mode (e.g. DsssRate1Mbps)", phyLayer);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
        LogComponentEnable("WifiMacQueueItem", LOG_LEVEL_ALL);
        LogComponentEnable("WifiHelper", LOG_LEVEL_ALL);
        LogComponentEnable("YansWifiChannel", LOG_LEVEL_ALL);
        LogComponentEnable("WifiPhy", LOG_LEVEL_ALL);
        LogComponentEnable("WifiNetDevice", LOG_LEVEL_ALL);
        LogComponentEnable("MeshPointDevice", LOG_LEVEL_ALL);
        LogComponentEnable("OnOffApplication", LOG_LEVEL_ALL);
    }

    // Create a 5x5 grid (25 nodes)
    uint32_t rows = 5;
    uint32_t columns = 5;
    NodeContainer nodes;
    nodes.Create(rows * columns);

    // Mobility configuration using GridPositionAllocator
    Ptr<GridPositionAllocator> gridAlloc = CreateObject<GridPositionAllocator>();
    gridAlloc->SetMinX(0.0);
    gridAlloc->SetMinY(0.0);
    gridAlloc->SetDeltaX(distance);
    gridAlloc->SetDeltaY(distance);
    gridAlloc->SetGridWidth(columns);
    gridAlloc->SetLayoutType(GridPositionAllocator::ROW_MAJOR);

    MobilityModel mobility;
    mobility.SetPositionAllocator(gridAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    InstallMobilityModelHelper mobilityHelper;
    mobilityHelper.Install(nodes);

    // Wifi configuration
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy;
    wifiPhy.Set("TxPowerStart", DoubleValue(16));
    wifiPhy.Set("TxPowerEnd", DoubleValue(16));
    wifiPhy.Set("TxGain", DoubleValue(0));
    wifiPhy.Set("RxGain", DoubleValue(0));
    wifiPhy.Set("RxNoiseFigure", DoubleValue(7));
    wifiPhy.Set("CcaMode1Threshold", DoubleValue(-95)); // RSSI cutoff

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Internet stack and routing with OLSR
    InternetStackHelper internet;
    OlsrHelper olsr;
    Ipv4ListRoutingHelper list;
    list.Add(olsr, 10);
    internet.SetRoutingHelper(list);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipAddrs;
    ipAddrs.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipAddrs.Assign(devices);

    // Set up OnOffApplication from source to sink
    uint16_t port = 9;
    OnOffHelper clientHelper("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(sinkNode), port)));
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("1kbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
    clientHelper.SetAttribute("MaxBytes", UintegerValue(packetSize * numPackets));

    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(sourceNode));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Sink (server) setup
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = sinkHelper.Install(nodes.Get(sinkNode));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // Tracing
    if (tracing) {
        AsciiTraceHelper ascii;
        wifiPhy.EnableAsciiAll(ascii.CreateFileStream("wireless-grid.tr"));
        wifiPhy.EnablePcapAll("wireless-grid", false);
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}