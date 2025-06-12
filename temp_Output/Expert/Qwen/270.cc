#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Dot11sMeshSimulation");

int main(int argc, char *argv[])
{
    uint32_t packetCount = 5;
    uint32_t packetSize = 512;
    double simulationTime = 10.0;
    double distance = 5.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("packetCount", "Number of packets to send", packetCount);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    // Create mesh nodes
    NodeContainer meshNodes;
    meshNodes.Create(3);

    // Setup mesh helper
    MeshHelper meshHelper = MeshHelper::Default();
    meshHelper.SetStackInstaller("ns3::Dot11sStack");
    meshHelper.SetMacType("Rhc");
    meshHelper.SetNumberOfInterfaces(1);

    NetDeviceContainer meshDevices = meshHelper.Install(meshNodes);

    // Setup mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
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

    // Setup UDP echo server on the last node
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(meshNodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));

    // Setup UDP echo client on the first node
    UdpEchoClientHelper echoClient(interfaces.GetAddress(2), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(packetCount));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps = echoClient.Install(meshNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime));

    // Enable PCAP tracing
    AsciiTraceHelper ascii;
    meshHelper.EnableAsciiAll(ascii.CreateFileStream("dot11s-mesh.tr"));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}