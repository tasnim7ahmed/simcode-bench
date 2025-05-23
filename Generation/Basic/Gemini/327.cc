#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set up logging for relevant components
    LogComponentEnable("OlsrHelper", LOG_LEVEL_INFO);
    LogComponentEnable("OlsrRoutingProtocol", LOG_LEVEL_DEBUG);
    LogComponentEnable("UdpClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Simulation parameters
    uint32_t numNodes = 6;
    double simulationTime = 10.0; // seconds
    double areaX = 100.0;         // meters
    double areaY = 100.0;         // meters

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Install mobility model (RandomWaypointMobilityModel)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue("ns3::UniformRandomVariable[Min=5.0|Max=15.0]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"), // No pause time
                              "Bounds", RectangleValue(Rectangle(-areaX / 2, areaX / 2, -areaY / 2, areaY / 2))); // 100x100m area centered at (0,0)
    
    // Set initial position allocator for nodes within the area
    mobility.SetPositionAllocator("ns3::UniformDiscPositionAllocator",
                                  "X", StringValue("0.0"),
                                  "Y", StringValue("0.0"),
                                  "Rho", StringValue("50.0")); // Place within a 50m radius disc (100m diameter)
    mobility.Install(nodes);

    // Install Internet Stack with OLSR routing protocol
    OlsrHelper olsr;
    Ipv4ListRoutingHelper listRouting;
    listRouting.Add(olsr, 10); // Add OLSR with a priority of 10

    InternetStackHelper internet;
    internet.SetRoutingHelper(listRouting);
    internet.Install(nodes);

    // Configure Wifi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ); // Choose a modern WiFi standard

    YansWifiPhyHelper phy;
    phy.Set("TxPowerStart", DoubleValue(10)); // Set transmission power
    phy.Set("TxPowerEnd", DoubleValue(10));
    phy.Set("TxPowerLevels", UintegerValue(1)); // Constant Tx power

    YansWifiChannelHelper channel;
    channel.SetPropagationLossModel("ns3::FriisPropagationLossModel");
    channel.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac"); // Ad-hoc network configuration

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Setup UDP traffic: Node 0 sends to Node 5
    uint16_t port = 9;
    
    // UDP Client (Node 0)
    UdpClientHelper client(interfaces.GetAddress(5), port); // Destination: Node 5's IP, port 9
    client.SetAttribute("MaxPackets", UintegerValue(1));    // Send a single packet
    client.SetAttribute("Interval", TimeValue(Seconds(1.0))); // Interval (not critical for 1 packet)
    client.SetAttribute("PacketSize", UintegerValue(1024)); // 1024 bytes packet size

    ApplicationContainer clientApps = client.Install(nodes.Get(0)); // Install on Node 0
    clientApps.Start(Seconds(1.0)); // Start sending at 1 second
    clientApps.Stop(Seconds(simulationTime));

    // UDP Server (Node 5)
    UdpServerHelper server(port); // Listen on port 9
    ApplicationContainer serverApps = server.Install(nodes.Get(5)); // Install on Node 5
    serverApps.Start(Seconds(0.5)); // Start listening slightly before client sends
    serverApps.Stop(Seconds(simulationTime));

    // Enable OLSR routing table printing to a trace file
    // This will log routing events and table changes, fulfilling the requirement to "print routing behavior"
    olsr.EnableAsciiAll("olsr-routing.tr", devices);

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}