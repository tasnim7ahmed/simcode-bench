#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/pcap-file-wrapper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiGridAdhocExample");

int main (int argc, char *argv[])
{
    uint32_t gridWidth = 5;
    uint32_t gridHeight = 5;
    double gridDistance = 100.0;
    std::string phyMode = "DsssRate11Mbps";
    bool verbose = false;
    uint32_t packetSize = 1000;
    uint32_t numPackets = 1;
    uint32_t sourceNode = 24;
    uint32_t sinkNode = 0;
    bool enablePcap = false;
    bool enableAscii = false;
    double rssiCutoffDbm = -85.0;

    CommandLine cmd;
    cmd.AddValue("gridWidth", "Number of nodes on x-axis", gridWidth);
    cmd.AddValue("gridHeight", "Number of nodes on y-axis", gridHeight);
    cmd.AddValue("phyMode", "802.11b PHY mode", phyMode);
    cmd.AddValue("gridDistance", "distance (m) between nodes", gridDistance);
    cmd.AddValue("packetSize", "Size of packets sent", packetSize);
    cmd.AddValue("numPackets", "Number of packets to send", numPackets);
    cmd.AddValue("sourceNode", "Node sending packets", sourceNode);
    cmd.AddValue("sinkNode", "Node receiving packets", sinkNode);
    cmd.AddValue("verbose", "Enable verbose logging", verbose);
    cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
    cmd.AddValue("enableAscii", "Enable ASCII tracing", enableAscii);
    cmd.AddValue("rssiCutoffDbm", "RSSI cutoff (dBm); below this, packet is dropped", rssiCutoffDbm);
    cmd.Parse(argc, argv);

    uint32_t numNodes = gridWidth * gridHeight;
    if (sourceNode >= numNodes) sourceNode = numNodes - 1;
    if (sinkNode >= numNodes) sinkNode = 0;

    if (verbose)
    {
        LogComponentEnable("WifiGridAdhocExample", LOG_LEVEL_INFO);
        LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
        LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create (numNodes);

    // Position allocator in a grid
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(gridDistance),
                                  "DeltaY", DoubleValue(gridDistance),
                                  "GridWidth", UintegerValue(gridWidth),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (nodes);

    // Configure 802.11b WiFi in Adhoc mode
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();

    Ptr<YansWifiChannel> wifiChannel = channel.Create();
    phy.SetChannel(wifiChannel);

    // Set RSSI cutoff for wireless transmission effects
    phy.Set ("RxSensitivity", DoubleValue(rssiCutoffDbm));
    phy.Set ("CcaMode1Threshold", DoubleValue(rssiCutoffDbm));

    WifiMacHelper mac;
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue (phyMode),
                                 "ControlMode", StringValue (phyMode));
    mac.SetType ("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

    // Internet stack with OLSR
    OlsrHelper olsr;
    InternetStackHelper internet;
    Ipv4ListRoutingHelper list;
    list.Add (olsr, 10);
    list.Add (Ipv4StaticRoutingHelper(), 0);
    internet.SetRoutingHelper (list);
    internet.Install(nodes);

    // Assign IPs
    Ipv4AddressHelper address;
    address.SetBase ("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install PacketSink (UDP) on sink node
    uint16_t udpPort = 5000;
    Address sinkAddress (InetSocketAddress (interfaces.GetAddress(sinkNode), udpPort));
    PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny(), udpPort));
    ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get(sinkNode));
    sinkApp.Start (Seconds (0.0));
    sinkApp.Stop (Seconds (20.0));

    // Install OnOffApplication on source node
    OnOffHelper onoff ("ns3::UdpSocketFactory", sinkAddress);
    onoff.SetAttribute ("PacketSize", UintegerValue(packetSize));
    onoff.SetAttribute ("DataRate", StringValue("1Mbps"));
    onoff.SetAttribute ("MaxBytes", UintegerValue(packetSize * numPackets));
    onoff.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer srcApp = onoff.Install (nodes.Get(sourceNode));
    srcApp.Start (Seconds (1.0));
    srcApp.Stop (Seconds (20.0));

    // Tracing
    if (enablePcap)
    {
        phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
        phy.EnablePcap ("wifi-grid-adhoc", devices, true);
    }
    if (enableAscii)
    {
        AsciiTraceHelper ascii;
        phy.EnableAsciiAll (ascii.CreateFileStream ("wifi-grid-adhoc.tr"));
    }

    Simulator::Stop (Seconds (21.0));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}