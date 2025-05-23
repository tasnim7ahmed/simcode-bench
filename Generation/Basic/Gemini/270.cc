#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Configure default attributes for 802.11s and Wifi
    Config::SetDefault("ns3::Dot11sStack::BeaconInterval", TimeValue(Seconds(1)));
    Config::SetDefault("ns3::Dot11sStack::Proactive", BooleanValue(true)); // Enable HWMP proactive mode

    // Use 802.11ac as the underlying Wifi standard
    WifiHelper wifiHelper = WifiHelper::Default();
    wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211ac);

    // Create three mesh nodes
    NodeContainer meshNodes;
    meshNodes.Create(3);

    // Configure Mobility Model - Grid layout
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0), // 5-meter spacing in X
                                  "DeltaY", DoubleValue(0.0), // No Y spacing (linear arrangement)
                                  "GridWidth", UintegerValue(3), // 3 nodes in a single row
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(meshNodes);

    // Configure Mesh Helper for 802.11s
    MeshHelper mesh;
    mesh.SetStackInstaller("ns3::Dot11sStack"); // Use the 802.11s stack

    // Configure WifiChannel and WifiPhy
    YansWifiChannelHelper channel;
    channel.SetPropagationLossModel("ns3::FriisPropagationLossModel");
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    // Set transmit power (e.g., 16.02 dBm = 40mW)
    phy.Set("TxPowerStart", DoubleValue(16.0206));
    phy.Set("TxPowerEnd", DoubleValue(16.0206));
    phy.Set("RxSensitivity", DoubleValue(-90.0));

    // Install mesh devices on nodes
    NetDeviceContainer meshDevices = mesh.Install(wifiHelper, phy, meshNodes);

    // Install Internet Stack on all nodes
    InternetStackHelper internet;
    internet.Install(meshNodes);

    // Assign IP addresses from 10.1.1.0/24 subnet
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer meshInterfaces = ipv4.Assign(meshDevices);

    // Set up UDP Echo Server on the last node (index 2)
    UdpEchoServerHelper echoServer(9); // Port 9
    ApplicationContainer serverApps = echoServer.Install(meshNodes.Get(2));
    serverApps.Start(Seconds(1.0));  // Server starts at 1 second
    serverApps.Stop(Seconds(10.0)); // Server stops at 10 seconds

    // Set up UDP Echo Client on the first node (index 0)
    // Client sends packets to the server's IP address
    Ipv4Address serverAddress = meshInterfaces.GetAddress(2); // IP of the last node
    UdpEchoClientHelper echoClient(serverAddress, 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));     // Send 5 packets
    echoClient.SetAttribute("PacketSize", UintegerValue(512));   // Packet size 512 bytes
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // 1-second interval between packets

    ApplicationContainer clientApps = echoClient.Install(meshNodes.Get(0));
    clientApps.Start(Seconds(2.0));  // Client starts at 2 seconds (after server)
    clientApps.Stop(Seconds(10.0)); // Client stops at 10 seconds

    // Set simulation duration
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}