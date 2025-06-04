#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ssid.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTcpMultipleClients");

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("WifiTcpMultipleClients", LOG_LEVEL_INFO);

    // Create 3 nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Setup the WiFi channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Setup the WiFi MAC layer
    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    WifiMacHelper mac;
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(Ssid("wifi-network")));

    // Install WiFi devices on the nodes
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Set up mobility for the nodes (Static positions)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install the Internet stack (IP/TCP stack)
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install a TCP server on node 2 (server side)
    uint16_t port = 9;
    TcpServerHelper tcpServer(port);
    ApplicationContainer serverApp = tcpServer.Install(nodes.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    // Install TCP clients on nodes 0 and 1 (client side)
    TcpClientHelper tcpClient1(interfaces.GetAddress(2), port);
    tcpClient1.SetAttribute("MaxPackets", UintegerValue(10));
    tcpClient1.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    tcpClient1.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApp1 = tcpClient1.Install(nodes.Get(0));
    clientApp1.Start(Seconds(2.0));
    clientApp1.Stop(Seconds(20.0));

    TcpClientHelper tcpClient2(interfaces.GetAddress(2), port);
    tcpClient2.SetAttribute("MaxPackets", UintegerValue(10));
    tcpClient2.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    tcpClient2.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApp2 = tcpClient2.Install(nodes.Get(1));
    clientApp2.Start(Seconds(3.0));
    clientApp2.Stop(Seconds(20.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
