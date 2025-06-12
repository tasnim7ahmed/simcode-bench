#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 4 mesh nodes
    NodeContainer meshNodes;
    meshNodes.Create(4);

    // Setup mesh helper
    MeshHelper mesh = MeshHelper::Default();
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.SetChannel(1);

    // Install mesh devices on all mesh nodes
    NetDeviceContainer meshDevices = mesh.Install(meshNodes);

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(meshNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(meshDevices);

    // Set up UDP Echo Server on node 3 (last node)
    uint16_t port = 9;  // UDP port number
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(meshNodes.Get(3));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Configure UDP clients on nodes 0, 1, and 2
    for (uint32_t i = 0; i < 3; ++i) {
        UdpEchoClientHelper echoClient(interfaces.GetAddress(3), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = echoClient.Install(meshNodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
    }

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}