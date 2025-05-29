#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer meshNodes;
    meshNodes.Create(3);

    // Set up Wifi mesh
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211s);

    MeshHelper mesh;
    mesh.SetStackInstaller(
        "ns3::Dot11sStack",
        "Mac/QosSupported", BooleanValue(false)
    );

    mesh.SetMacType("RandomStart");

    NetDeviceContainer meshDevices = mesh.Install(wifi, meshNodes);

    // Install mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(meshNodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(meshNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(meshDevices);

    // Create UDP server on node 2
    UdpServerHelper server(9);
    ApplicationContainer serverApp = server.Install(meshNodes.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Create UDP client on node 0 and send packets to node 2
    UdpClientHelper client(interfaces.GetAddress(2), 9);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    client.SetAttribute("PacketSize", UintegerValue(1024););

    ApplicationContainer clientApp = client.Install(meshNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable PCAP tracing
    wifi.EnablePcapAll("mesh-example");

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}