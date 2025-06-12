#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiSimpleExample");

int main(int argc, char *argv[])
{
    // Enable log components
    LogComponentEnable("WifiSimpleExample", LOG_LEVEL_INFO);

    // Create nodes for the network
    NodeContainer nodes;
    nodes.Create(2); // Two nodes: Client and Server

    // Set up mobility model (place nodes at fixed positions)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Set up WiFi helper for wireless communication
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    // Set up WiFi PHY (physical layer)
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(YansWifiChannelHelper::Default().Create());

    // Set up WiFi MAC (media access control)
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("ns-3-ssid")));

    // Install WiFi devices on nodes
    NetDeviceContainer devices;
    devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Install the internet stack (TCP/IP stack)
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Set up the TCP server application on Node 1 (server)
    uint16_t port = 9; // TCP port for communication
    TcpServerHelper tcpServer(port);
    ApplicationContainer serverApp = tcpServer.Install(nodes.Get(1)); // Install on Node 1 (Server)
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up the TCP client application on Node 0 (client)
    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0));             // Unlimited data transfer
    ApplicationContainer clientApp = bulkSendHelper.Install(nodes.Get(0)); // Install on Node 0 (Client)
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
