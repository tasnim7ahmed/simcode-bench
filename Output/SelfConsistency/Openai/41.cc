#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/applications-module.h"
#include "ns3/pcap-file.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiAdhocGrid");

int
main(int argc, char *argv[])
{
    // Default simulation parameters
    uint32_t gridWidth = 5;
    uint32_t gridHeight = 5;
    double gridDistance = 30.0;   // meters
    std::string phyMode = "DsssRate11Mbps";
    uint32_t packetSize = 1000;   // bytes
    uint32_t numPackets = 1;
    uint32_t sourceNode = 24;
    uint32_t sinkNode = 0;
    bool verbose = false;
    bool enablePcap = false;
    bool enableAscii = false;
    double rssiCutoffDbm = -85.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("gridWidth", "Number of nodes per row in grid", gridWidth);
    cmd.AddValue("gridHeight", "Number of nodes per column in grid", gridHeight);
    cmd.AddValue("gridDistance", "Meter spacing between nodes", gridDistance);
    cmd.AddValue("phyMode", "Wifi Physical layer mode (e.g., DsssRate11Mbps)", phyMode);
    cmd.AddValue("packetSize", "Size of each packet in bytes", packetSize);
    cmd.AddValue("numPackets", "Number of packets to send", numPackets);
    cmd.AddValue("sourceNode", "Index of sender node", sourceNode);
    cmd.AddValue("sinkNode", "Index of destination node", sinkNode);
    cmd.AddValue("verbose", "Enable verbose logging", verbose);
    cmd.AddValue("enablePcap", "Enable pcap tracing", enablePcap);
    cmd.AddValue("enableAscii", "Enable ASCII tracing", enableAscii);
    cmd.AddValue("rssiCutoffDbm", "RSSI cutoff in dBm (signals below this are lost)", rssiCutoffDbm);
    cmd.Parse(argc, argv);

    uint32_t numNodes = gridWidth * gridHeight;

    // Sanity check for source/sink nodes
    if (sourceNode >= numNodes || sinkNode >= numNodes || sourceNode == sinkNode)
    {
        NS_LOG_UNCOND("Invalid source/sink node IDs. Using defaults: source=24, sink=0.");
        sourceNode = numNodes - 1;
        sinkNode = 0;
    }
    if (verbose)
    {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
        LogComponentEnable("WifiAdhocGrid", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(numNodes);

    // Position nodes in a grid
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(gridDistance),
                                 "DeltaY", DoubleValue(gridDistance),
                                 "GridWidth", UintegerValue(gridWidth),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Configure wifi PHY/MAC for adhoc
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue(phyMode),
                                "ControlMode", StringValue(phyMode));

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    channel.SetPropagationLoss("ns3::RangePropagationLossModel",
                               "MaxRange", DoubleValue(std::pow(10.0, (27.55 - (20 * std::log10(2.4e9)) + std::abs(rssiCutoffDbm))/20.0)));
    channel.AddPropagationLoss("ns3::LogDistancePropagationLossModel"); // Basic path loss

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());
    phy.Set("RxGain", DoubleValue(0.0));
    phy.SetErrorRateModel("ns3::YansErrorRateModel");

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Internet stack + OLSR routing
    InternetStackHelper internet;
    OlsrHelper olsr;
    internet.SetRoutingHelper(olsr);
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // UDP server (sink)
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(sinkNode));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(20.0));

    // UDP client (source)
    UdpClientHelper client(interfaces.GetAddress(sinkNode), port);
    client.SetAttribute("MaxPackets", UintegerValue(numPackets));
    client.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp = client.Install(nodes.Get(sourceNode));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(20.0));

    // Optional tracing
    if (enablePcap)
    {
        phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        phy.EnablePcapAll("wifi-adhoc-grid");
    }
    Ptr<OutputStreamWrapper> asciiStream;
    if (enableAscii)
    {
        AsciiTraceHelper ascii;
        asciiStream = ascii.CreateFileStream("wifi-adhoc-grid.tr");
        phy.EnableAsciiAll(asciiStream);
    }

    // Run simulation
    Simulator::Stop(Seconds(21.0));
    Simulator::Run();
    Simulator::Destroy();

    // Report summary
    uint32_t totalRx = DynamicCast<UdpServer>(serverApp.Get(0))->GetReceived();
    std::cout << "Received " << totalRx << " packets at node " << sinkNode << std::endl;

    return 0;
}