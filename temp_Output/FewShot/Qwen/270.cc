#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mesh-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create nodes (Mesh network with 3 nodes)
    NodeContainer nodes;
    nodes.Create(3);

    // Setup MeshHelper with Dot11s protocol
    MeshHelper mesh = MeshHelper::Default();
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.SetMacType("Rf", StringValue("5ms"));
    mesh.SetNumberOfInterfaces(1);

    // Install mesh devices to the nodes
    NetDeviceContainer meshDevices = mesh.Install(nodes);

    // Setup mobility model for grid layout
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install Internet stack on all nodes
    InternetStackHelper internetStack;
    internetStack.Install(nodes);

    // Assign IP addresses to mesh interfaces
    Ipv4AddressHelper ipAddresses;
    ipAddresses.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipAddresses.Assign(meshDevices);

    // Set up UDP Echo Server on last node (node index 2)
    uint16_t port = 9; // UDP echo server port
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP Echo Client on first node (node index 0) sending to last node
    UdpEchoClientHelper echoClient(interfaces.GetAddress(2), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable logging for debugging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}