#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for receiver applications
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create one sender and multiple receivers (e.g., 1 sender + 3 receivers)
    NodeContainer nodes;
    nodes.Create(4);
    Ptr<Node> sender = nodes.Get(0);
    NodeContainer receivers = NodeContainer(nodes.Get(1), nodes.Get(2), nodes.Get(3));

    // Setup Wi-Fi PHY and MAC
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    YansWifiPhyHelper phy;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac"); // No centralized access point

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up a global broadcast address
    Ipv4Address broadcastAddr = interfaces.GetAddress(1).GetSubnetDirectedBroadcast();

    // Configure UDP Echo Server on all receivers
    uint16_t port = 9; // UDP port number
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(receivers);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Configure UDP Echo Client on the sender node
    UdpEchoClientHelper echoClient(broadcastAddr, port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = echoClient.Install(sender);
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}