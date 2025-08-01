// This program configures a grid (default 5x5) of nodes on an
// 802.11b physical layer, with
// 802.11b NICs in adhoc mode, and by default, sends one packet of 1000
// (application) bytes to node 1.
//
// The default layout is like this, on a 2-D grid.
//
// n20  n21  n22  n23  n24
// n15  n16  n17  n18  n19
// n10  n11  n12  n13  n14
// n5   n6   n7   n8   n9
// n0   n1   n2   n3   n4
//
// the layout is affected by the parameters given to GridPositionAllocator;
// by default, GridWidth is 5 (nodes per row) and numNodes is 25..
//
// There are a number of command-line options available to control
// the default behavior.  The list of available command-line options
// can be listed with the following command:
// ./ns3 run "wifi-simple-adhoc-grid --help"
//
// Note that all ns-3 attributes (not just the ones exposed in the below
// script) can be changed at command line; see the ns-3 documentation.
//
// For instance, for this configuration, the physical layer will
// stop successfully receiving packets when distance increases beyond
// the default of 100m.  The cutoff is around 116m; below that value, the
// received signal strength falls below the (default) RSSI limit of -82 dBm
// used by Wi-Fi's threshold preamble detection running.
//
// To see this effect, try running at a larger distance, and no packet
// reception will be reported:
//
// ./ns3 run "wifi-simple-adhoc-grid --distance=200"
//
// The default path through the topology will follow the following node
// numbers:  24->23->18->13->12->11->10->5->0
//
// To see this, the following Bash commands will list the UDP packet
// transmissions hop-by-hop, if tracing is enabled:
//
// ./ns3 run "wifi-simple-adhoc-grid --tracing=1"
// grep ^t wifi-simple-adhoc-grid.tr  | grep Udp | grep -v olsr | less
//
// By changing the distance to a smaller value, more nodes can be reached
// by each transmission, and the number of forwarding hops will decrease.
//
// The source node and sink node can be changed like this:
//
// ./ns3 run "wifi-simple-adhoc-grid --sourceNode=20 --sinkNode=10"
//
// This script can also be helpful to put the Wifi layer into verbose
// logging mode; this command will turn on all wifi logging:
//
// ./ns3 run "wifi-simple-adhoc-grid --verbose=1"
//
// By default, trace file writing is off-- to enable it, try:
// ./ns3 run "wifi-simple-adhoc-grid --tracing=1"
//
// When you are done tracing, you will notice many pcap trace files
// in your directory.  If you have tcpdump installed, you can try this:
//
// tcpdump -r wifi-simple-adhoc-grid-0-0.pcap -nn -tt
//
// or you can examine the text-based trace wifi-simple-adhoc-grid.tr with
// an editor.
//

#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/double.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/log.h"
#include "ns3/mobility-helper.h"
#include "ns3/mobility-model.h"
#include "ns3/olsr-helper.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiSimpleAdhocGrid");

/**
 * Function called when a packet is received.
 *
 * \param socket The receiving socket.
 */
void
ReceivePacket(Ptr<Socket> socket)
{
    while (socket->Recv())
    {
        NS_LOG_UNCOND("Received one packet!");
    }
}

/**
 * Generate traffic.
 *
 * \param socket The sending socket.
 * \param pktSize The packet size.
 * \param pktCount The packet count.
 * \param pktInterval The interval between two packets.
 */
static void
GenerateTraffic(Ptr<Socket> socket, uint32_t pktSize, uint32_t pktCount, Time pktInterval)
{
    if (pktCount > 0)
    {
        socket->Send(Create<Packet>(pktSize));
        Simulator::Schedule(pktInterval,
                            &GenerateTraffic,
                            socket,
                            pktSize,
                            pktCount - 1,
                            pktInterval);
    }
    else
    {
        socket->Close();
    }
}

int
main(int argc, char* argv[])
{
    std::string phyMode{"DsssRate1Mbps"};
    meter_u distance{100};
    uint32_t packetSize{1000}; // bytes
    uint32_t numPackets{1};
    uint32_t numNodes{25}; // by default, 5x5
    uint32_t sinkNode{0};
    uint32_t sourceNode{24};
    Time interPacketInterval{"1s"};
    bool verbose{false};
    bool tracing{false};

    CommandLine cmd(__FILE__);
    cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
    cmd.AddValue("distance", "distance (m)", distance);
    cmd.AddValue("packetSize", "size of application packet sent", packetSize);
    cmd.AddValue("numPackets", "number of packets generated", numPackets);
    cmd.AddValue("interval", "interval (seconds) between packets", interPacketInterval);
    cmd.AddValue("verbose", "turn on all WifiNetDevice log components", verbose);
    cmd.AddValue("tracing", "turn on ascii and pcap tracing", tracing);
    cmd.AddValue("numNodes", "number of nodes", numNodes);
    cmd.AddValue("sinkNode", "Receiver node number", sinkNode);
    cmd.AddValue("sourceNode", "Sender node number", sourceNode);
    cmd.Parse(argc, argv);

    // Fix non-unicast data rate to be the same as that of unicast
    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue(phyMode));

    NodeContainer c;
    c.Create(numNodes);

    // The below set of helpers will help us to put together the wifi NICs we want
    WifiHelper wifi;
    if (verbose)
    {
        WifiHelper::EnableLogComponents(); // Turn on all Wifi logging
    }

    YansWifiPhyHelper wifiPhy;
    // set it to zero; otherwise, gain will be added
    wifiPhy.Set("RxGain", DoubleValue(-10));
    // ns-3 supports RadioTap and Prism tracing extensions for 802.11b
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
    wifiPhy.SetChannel(wifiChannel.Create());

    // Add an upper mac and disable rate control
    WifiMacHelper wifiMac;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue(phyMode),
                                 "ControlMode",
                                 StringValue(phyMode));
    // Set it to adhoc mode
    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, c);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX",
                                  DoubleValue(0.0),
                                  "MinY",
                                  DoubleValue(0.0),
                                  "DeltaX",
                                  DoubleValue(distance),
                                  "DeltaY",
                                  DoubleValue(distance),
                                  "GridWidth",
                                  UintegerValue(5),
                                  "LayoutType",
                                  StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(c);

    // Enable OLSR
    OlsrHelper olsr;
    Ipv4StaticRoutingHelper staticRouting;

    Ipv4ListRoutingHelper list;
    list.Add(staticRouting, 0);
    list.Add(olsr, 10);

    InternetStackHelper internet;
    internet.SetRoutingHelper(list); // has effect on the next Install ()
    internet.Install(c);

    Ipv4AddressHelper ipv4;
    NS_LOG_INFO("Assign IP Addresses.");
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i = ipv4.Assign(devices);

    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    Ptr<Socket> recvSink = Socket::CreateSocket(c.Get(sinkNode), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 80);
    recvSink->Bind(local);
    recvSink->SetRecvCallback(MakeCallback(&ReceivePacket));

    Ptr<Socket> source = Socket::CreateSocket(c.Get(sourceNode), tid);
    InetSocketAddress remote = InetSocketAddress(i.GetAddress(sinkNode, 0), 80);
    source->Connect(remote);

    if (tracing)
    {
        AsciiTraceHelper ascii;
        wifiPhy.EnableAsciiAll(ascii.CreateFileStream("wifi-simple-adhoc-grid.tr"));
        wifiPhy.EnablePcap("wifi-simple-adhoc-grid", devices);
        // Trace routing tables
        Ptr<OutputStreamWrapper> routingStream =
            Create<OutputStreamWrapper>("wifi-simple-adhoc-grid.routes", std::ios::out);
        Ipv4RoutingHelper::PrintRoutingTableAllEvery(Seconds(2), routingStream);
        Ptr<OutputStreamWrapper> neighborStream =
            Create<OutputStreamWrapper>("wifi-simple-adhoc-grid.neighbors", std::ios::out);
        Ipv4RoutingHelper::PrintNeighborCacheAllEvery(Seconds(2), neighborStream);

        // To do-- enable an IP-level trace that shows forwarding events only
    }

    // Give OLSR time to converge-- 30 seconds perhaps
    Simulator::Schedule(Seconds(30.0),
                        &GenerateTraffic,
                        source,
                        packetSize,
                        numPackets,
                        interPacketInterval);

    // Output what we are doing
    NS_LOG_UNCOND("Testing from node " << sourceNode << " to " << sinkNode << " with grid distance "
                                       << distance);

    Simulator::Stop(Seconds(33.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
