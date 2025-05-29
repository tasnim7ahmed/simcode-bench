#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mesh-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    // Create 3 nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Set up WiFi physical and channel
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    // Set up Mesh
    MeshHelper mesh;
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.SetMacType("RandomStart", TimeValue(Seconds(0.1)));
    mesh.SetStandard(WIFI_STANDARD_80211s);

    NetDeviceContainer meshDevices = mesh.Install(wifiPhy, nodes);

    // Mobility: arrange nodes in a line
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(50.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install Internet stack
    InternetStackHelper internetStack;
    internetStack.Install(nodes);

    // Assign IP addresses to the mesh interfaces
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer meshInterfaces = address.Assign(meshDevices);

    // UDP Server on Node 2 (receives packets)
    uint16_t udpPort = 9;
    UdpServerHelper udpServer(udpPort);
    ApplicationContainer serverApp = udpServer.Install(nodes.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP Client on Node 0 (sends to Node 2)
    UdpClientHelper udpClient(meshInterfaces.GetAddress(2), udpPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(20));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = udpClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}