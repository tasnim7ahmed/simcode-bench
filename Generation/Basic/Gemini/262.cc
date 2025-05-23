#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mesh-module.h"

using namespace ns3;

int
main(int argc, char* argv[])
{
    // Set default time resolution to nanoseconds
    Time::SetResolution(NanoSeconds);

    // Disable RtsCts and Fragmentation for simplicity if not explicitly required,
    // though for mesh they can be important.
    // Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue (999999));
    // Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", UintegerValue (999999));

    // Create 5 nodes
    NodeContainer nodes;
    nodes.Create(5);

    // Install the Internet stack on the nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Configure Wi-Fi devices for 802.11s mesh
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ); // 802.11s can operate over 802.11a/g/n

    YansWifiPhyHelper phy;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure MeshHelper for 802.11s
    MeshHelper mesh;
    mesh.SetStackInstaller("ns3::Dot11sStackHelper"); // Explicitly set 802.11s stack
    mesh.SetNumberOfInterfaces(1); // One Wi-Fi interface per node
    
    // Install Wi-Fi devices with 802.11s mesh capabilities
    NetDeviceContainer devices = mesh.Install(wifi, phy, nodes);

    // Assign IPv4 addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Configure mobility model (RandomWalk2dMobilityModel)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(nodes);

    // Setup UDP Echo Server on the last node (Node 4)
    UdpEchoServerHelper echoServer(9); // Port 9
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(4));
    serverApps.Start(Seconds(1.0)); // Server starts at 1 second
    serverApps.Stop(Seconds(10.0));

    // Setup UDP Echo Client on the first node (Node 0)
    // Client sends to the server's IP address (Node 4's IP)
    UdpEchoClientHelper echoClient(interfaces.GetAddress(4), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0)); // Client starts at 2 seconds
    clientApps.Stop(Seconds(10.0));

    // Enable tracing if needed
    // AsciiTraceHelper ascii;
    // phy.EnableAsciiAll(ascii.CreateFileStream("mesh-80211s.tr"));
    // phy.EnablePcapAll("mesh-80211s", true);

    // Run the simulation
    Simulator::Stop(Seconds(10.0)); // Simulation runs for 10 seconds
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}