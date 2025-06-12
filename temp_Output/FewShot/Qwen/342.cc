#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for TCP and WiFi
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("TcpSocketBase", LOG_LEVEL_INFO);
    LogComponentEnable("WifiMacQueueItem", LOG_LEVEL_DEBUG);

    // Create two nodes (client and server)
    NodeContainer nodes;
    nodes.Create(2);

    // Setup WiFi with 802.11b standard
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    // Use AARF rate control algorithm
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    // Configure the PHY layer
    YansWifiPhyHelper phy;
    phy.SetChannel(YansWifiChannelHelper::Default().Create());

    // Configure the MAC layer
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");  // No access point, ad hoc mode

    // Install devices on nodes
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Set up static mobility models with positions
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up TCP receiver (server) on node 1
    uint16_t port = 9;  // Well-known port number
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(20.0));

    // Set up TCP sender (client) on node 0
    AddressValue remoteAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    BulkSendHelper bulkSend("ns3::TcpSocketFactory", Address());
    bulkSend.SetAttribute("Remote", remoteAddress);
    bulkSend.SetAttribute("MaxBytes", UintegerValue(100 * 512));  // Send 100 packets of 512 bytes
    bulkSend.SetAttribute("SendSize", UintegerValue(512));

    ApplicationContainer clientApp = bulkSend.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(20.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}