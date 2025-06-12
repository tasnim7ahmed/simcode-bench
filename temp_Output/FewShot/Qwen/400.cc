#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set simulation time
    double simTime = 10.0;

    // Enable logging for TCP applications
    LogComponentEnable("TcpServer", LOG_LEVEL_INFO);
    LogComponentEnable("TcpClient", LOG_LEVEL_INFO);

    // Create two Wi-Fi nodes
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);

    // Configure the Wi-Fi channel and MAC layer
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();

    // Install Wi-Fi devices
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Set up mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(staDevices);

    // Set up TCP Server on node 1
    uint16_t port = 9;  // Well-known port number
    TcpServerHelper tcpServer(port);
    ApplicationContainer serverApp = tcpServer.Install(wifiStaNodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simTime));

    // Set up TCP Client on node 0
    TcpClientHelper tcpClient(interfaces.GetAddress(1), port);
    tcpClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF)); // Infinite packets
    tcpClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    tcpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = tcpClient.Install(wifiStaNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(simTime));

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}