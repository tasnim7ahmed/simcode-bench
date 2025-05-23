#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/mesh-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h" // For WifiMacType

using namespace ns3;

int main(int argc, char *argv[])
{
    // 1. Configure logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("MeshHelper", LOG_LEVEL_INFO);
    LogComponentEnable("WifiPhy", LOG_LEVEL_INFO);

    // Simulation parameters
    double simulationTime = 10.0;     // Total simulation time in seconds
    uint32_t payloadSize = 1024;      // UDP packet payload size in bytes
    double serverStartTime = 1.0;     // Server application start time in seconds
    double clientStartTime = 2.0;     // Client application start time in seconds
    double interPacketInterval = 1.0; // Time interval between packets in seconds
    uint32_t maxPackets = 100;        // Maximum number of packets for clients to send

    // 2. Create Nodes
    NodeContainer nodes;
    nodes.Create(3); // Node 0 (Node 1), Node 1 (Node 2), Node 2 (Node 3)

    // 3. Install Mobility Model (static positions for a simple mesh)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);
    nodes.Get(0)->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));
    nodes.Get(1)->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(10.0, 0.0, 0.0));
    nodes.Get(2)->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(20.0, 0.0, 0.0));

    // 4. Configure Mesh Network Devices
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ); // Or WIFI_PHY_STANDARD_80211s for mesh

    YansWifiPhyHelper wifiPhy;
    // Set typical transmit power for reasonable range
    wifiPhy.Set("TxPowerStart", DoubleValue(16.0));
    wifiPhy.Set("TxPowerEnd", DoubleValue(16.0));
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
    // Set MAC type to MeshWifiMac with Random Access (802.11s style)
    wifiMac.SetType("ns3::MeshWifiMac",
                    "MeshType", EnumValue(MeshWifiMac::MESH_RANDOM_ACCESS));

    MeshHelper mesh;
    mesh.SetNumberOfChannels(1); // Use a single channel for all nodes
    mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS); // No effect with 1 channel
    
    // Set a mesh routing protocol. AOLS is suitable for mesh.
    mesh.SetRoutingProtocol("ns3::Aolsr"); 

    // Install mesh devices on nodes
    NetDeviceContainer devices = mesh.Install(wifiPhy, wifiMac, nodes);

    // 5. Install Internet Stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // 6. Assign IP Addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Populate routing tables for proper IP routing across the mesh
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // 7. Setup UDP Applications

    // --- Node 1 (index 0) sends to Node 2 (index 1) ---
    // Server on Node 2
    UdpEchoServerHelper echoServer1(9); // Port 9
    ApplicationContainer serverApps1 = echoServer1.Install(nodes.Get(1)); // Node 2
    serverApps1.Start(Seconds(serverStartTime));
    serverApps1.Stop(Seconds(simulationTime));

    // Client on Node 1 sends to Node 2
    UdpEchoClientHelper echoClient1(interfaces.GetAddress(1), 9); // Node 2's IP, Port 9
    echoClient1.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(interPacketInterval)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(payloadSize));
    ApplicationContainer clientApps1 = echoClient1.Install(nodes.Get(0)); // Node 1
    clientApps1.Start(Seconds(clientStartTime));
    clientApps1.Stop(Seconds(simulationTime));

    // --- Node 2 (index 1) sends to Node 3 (index 2) ---
    // Server on Node 3
    UdpEchoServerHelper echoServer2(10); // Use a different port: Port 10
    ApplicationContainer serverApps2 = echoServer2.Install(nodes.Get(2)); // Node 3
    serverApps2.Start(Seconds(serverStartTime));
    serverApps2.Stop(Seconds(simulationTime));

    // Client on Node 2 sends to Node 3
    UdpEchoClientHelper echoClient2(interfaces.GetAddress(2), 10); // Node 3's IP, Port 10
    echoClient2.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(interPacketInterval)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(payloadSize));
    ApplicationContainer clientApps2 = echoClient2.Install(nodes.Get(1)); // Node 2
    clientApps2.Start(Seconds(clientStartTime));
    clientApps2.Stop(Seconds(simulationTime));

    // 8. Run Simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}