#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);

    // Create a server node and multiple client nodes
    NodeContainer serverNode;
    serverNode.Create(1);

    NodeContainer clientNodes;
    clientNodes.Create(3);  // Three client nodes

    NodeContainer allNodes = NodeContainer(serverNode, clientNodes);

    // Create a Wi-Fi helper for the physical and MAC layers
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);

    // Set up the PHY layer
    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    // Set up the MAC layer (use default ssid for shared network)
    NqosWifiMacHelper wifiMac;
    wifiMac.SetType("ns3::StaWifiMac");
    NetDeviceContainer clientDevices = wifi.Install(wifiPhy, wifiMac, clientNodes);

    wifiMac.SetType("ns3::ApWifiMac");
    NetDeviceContainer serverDevice = wifi.Install(wifiPhy, wifiMac, serverNode);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(serverDevice);
    interfaces.Add(address.Assign(clientDevices));

    // Set up UDP Echo Server on the server node
    uint16_t port = 9;  // Well-known echo port
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(serverNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP Echo Clients on each client node
    for (uint32_t i = 0; i < clientNodes.GetN(); ++i) {
        UdpEchoClientHelper echoClient(interfaces.GetAddress(0), port);  // server's IP is first assigned address
        echoClient.SetAttribute("MaxPackets", UintegerValue(5));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApp = echoClient.Install(clientNodes.Get(i));
        clientApp.Start(Seconds(2.0 + i));  // staggered start times
        clientApp.Stop(Seconds(10.0));
    }

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}