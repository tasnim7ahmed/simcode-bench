#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mesh-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MeshUdpSimulation");

int main(int argc, char *argv[])
{
    // Log components
    LogComponentEnable("UdpClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServerApplication", LOG_LEVEL_INFO);

    // Set simulation time
    double serverStartTime = 1.0;
    double clientStartTime = 2.0;
    double stopTime = 10.0;

    // Mesh network setup
    MeshHelper mesh = MeshHelper::Default();

    // Create nodes
    NodeContainer meshNodes;
    meshNodes.Create(4);

    // Install mesh stack
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.SetMacType("Rsn");
    mesh.SetChannelWidth(20);
    NetDeviceContainer meshDevices = mesh.Install(meshNodes);

    // Setup mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(100.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(4),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(meshNodes);

    // Install internet stack
    InternetStackHelper internet;
    internet.Install(meshNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(meshDevices);

    // Set up UDP server on the last node (node 3)
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(meshNodes.Get(3));
    serverApps.Start(Seconds(serverStartTime));
    serverApps.Stop(Seconds(stopTime));

    // Set up UDP clients on first three nodes
    UdpClientHelper client(interfaces.GetAddress(3), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    for (uint32_t i = 0; i < 3; ++i)
    {
        ApplicationContainer clientApp = client.Install(meshNodes.Get(i));
        clientApp.Start(Seconds(clientStartTime));
        clientApp.Stop(Seconds(stopTime));
    }

    // Enable ASCII and PCAP tracing
    AsciiTraceHelper ascii;
    mesh.EnableAsciiAll(ascii.CreateFileStream("mesh-wifi-udp.tr"));
    mesh.EnablePcapAll("mesh-wifi-udp");

    // Run simulation
    Simulator::Stop(Seconds(stopTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}