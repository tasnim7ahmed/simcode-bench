#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-helper.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create nodes for the mesh network
    NodeContainer nodes;
    nodes.Create(3);

    // Setup mesh helper with Random Access MAC and YansWifiPhy
    MeshHelper mesh = MeshHelper::Default();
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.SetMacType("RandomAccess");
    mesh.SetNumberOfInterfaces(1);

    // Install mesh devices
    NetDeviceContainer meshDevices = mesh.Install(WifiPhyHelper::Default(), nodes);
    
    // Enable logging for mesh devices
    LogComponentEnable("MeshPointDevice", LOG_LEVEL_ALL);

    // Install Internet stack
    InternetStackHelper internetStack;
    internetStack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipAddrs;
    ipAddrs.SetBase("10.0.0.0", "255.255.255.0");
    ipAddrs.Assign(meshDevices);

    // Set up UDP Echo Server on node 2 (IP address assigned to node index 2)
    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP Echo Client on node 1, sending to node 2
    UdpEchoClientHelper client(Ipv4Address("10.0.0.3"), port); // Assuming node 2 gets .3
    client.SetAttribute("MaxPackets", UintegerValue(5));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = client.Install(nodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Set mobility model to static
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(100.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}