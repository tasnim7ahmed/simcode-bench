#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for TCP server and client
    LogComponentEnable("TcpServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("TcpClientApplication", LOG_LEVEL_INFO);

    // Create nodes: 3 clients and 1 server
    NodeContainer clients;
    clients.Create(3);

    NodeContainer serverNode;
    serverNode.Create(1);

    NodeContainer allNodes = NodeContainer(clients, serverNode);

    // Create a Wi-Fi channel and install the PHY layer
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // Set up MAC layer with default SSID
    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    // Install Wi-Fi devices on all nodes
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, allNodes);

    // Set up AP node (server)
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, serverNode);

    // Mobility model: constant position
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(allNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer serverInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer clientInterfaces = address.Assign(staDevices);

    // Set up TCP Server on the server node
    uint16_t port = 5000;
    TcpServerHelper tcpServer(port);
    ApplicationContainer serverApp = tcpServer.Install(serverNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up TCP Clients on each client node
    for (uint32_t i = 0; i < clients.GetN(); ++i) {
        TcpClientHelper tcpClient(serverInterface.GetAddress(0), port);
        tcpClient.SetAttribute("MaxBytes", UintegerValue(1024)); // Send 1KB per client

        ApplicationContainer clientApp = tcpClient.Install(clients.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
    }

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}