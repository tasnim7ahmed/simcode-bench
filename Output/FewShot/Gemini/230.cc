#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging (optional)
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: one server and multiple clients
    NodeContainer serverNode;
    serverNode.Create(1);
    NodeContainer clientNodes;
    clientNodes.Create(2); // Example: 2 client nodes
    NodeContainer allNodes = NodeContainer(serverNode, clientNodes);

    // Set up Wi-Fi PHY and channel
    WifiHelper wifi;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure Wi-Fi MAC (using standard STA/AP configuration)
    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    // Server (AP) configuration
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(MilliSeconds(100)));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, serverNode);

    // Clients (STA) configuration
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, clientNodes);

    // Install the Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(apDevice, staDevices));

    // Server application (UDP Echo Server)
    uint16_t port = 9; // Standard echo port
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(serverNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Client applications (UDP Echo Client)
    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), port); // Server IP
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < clientNodes.GetN(); ++i) {
        clientApps.Add(echoClient.Install(clientNodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Mobility model (static positions)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(allNodes);

    // Run the simulation
    Simulator::Run(Seconds(10.0));
    Simulator::Destroy();

    return 0;
}