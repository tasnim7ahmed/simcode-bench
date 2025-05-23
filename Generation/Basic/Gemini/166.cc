#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-helper.h"
#include "ns3/applications-module.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging for relevant components to observe route discovery and application flow
    LogComponentEnable("ns3::aodv::RoutingProtocol", LOG_LEVEL_ALL);
    LogComponentEnable("ns3::aodv::RoutingTable", LOG_LEVEL_INFO); // INFO level for table updates
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Set up default values for Wifi (optional, but good practice)
    Config::SetDefault("ns3::WifiRemoteStationManager::NonQuietPeriod", TimeValue(Seconds(1.0)));

    // Create five nodes
    NodeContainer nodes;
    nodes.Create(5);

    // Configure node mobility for random placement in a 100x100 meter area
    MobilityHelper mobility;
    Ptr<RandomBoxPositionAllocator> positionAlloc = CreateObject<RandomBoxPositionAllocator>();
    positionAlloc->SetBox(Box(0, 100, 0, 100, 0, 0)); // X: 0-100, Y: 0-100, Z: 0 (2D placement)
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel"); // Nodes remain stationary after placement
    mobility.Install(nodes);

    // Install AODV routing protocol and Internet stack on all nodes
    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv);
    stack.Install(nodes);

    // Configure Wifi devices for ad-hoc mode
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n); // Using 802.11n standard

    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO); // Enable PCAP tracing

    // Configure a wireless channel
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationLossModel("ns3::FriisPropagationLossModel");
    wifiChannel.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
    wifiPhy.SetChannel(wifiChannel.Create());

    // Configure MAC layer for ad-hoc (IBSS) mode
    NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
    wifiMac.SetType("ns3::AdhocWifiMac");

    // Install Wifi devices on nodes
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Configure UDP Echo Server on node 1 (nodes.Get(1))
    uint16_t port = 9; // Echo server port
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1)); // Server is node 1
    serverApps.Start(Seconds(1.0)); // Server starts at 1 second
    serverApps.Stop(Seconds(10.0)); // Server stops at 10 seconds

    // Configure UDP Echo Client on node 0 (nodes.Get(0))
    // It targets the IP address of node 1's interface
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));    // Send 5 packets
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // 1 second interval between packets
    echoClient.SetAttribute("PacketSize", UintegerValue(1024)); // 1024 byte packets

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0)); // Client is node 0
    clientApps.Start(Seconds(2.0)); // Client starts sending at 2 seconds
    clientApps.Stop(Seconds(10.0)); // Client stops at 10 seconds

    // Enable packet tracing
    wifiPhy.EnablePcapAll("adhoc-aodv"); // Capture PCAP traces for all Wifi devices
    // Simulator::EnableAsciiTrace("adhoc-aodv-ascii", devices); // Optional: ASCII trace for higher-level events

    // Set simulation stop time
    Simulator::Stop(Seconds(11.0));

    // Run the simulation
    Simulator::Run();

    // Clean up
    Simulator::Destroy();

    return 0;
}