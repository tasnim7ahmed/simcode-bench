#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aarf-wifi-manager.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"

// Define a log component for this script
NS_LOG_COMPONENT_DEFINE("WifiMeshAdHoc");

int main(int argc, char* argv[])
{
    // Set up command line arguments
    uint32_t numNodes = 5;
    double simulationTime = 10.0; // seconds
    uint32_t payloadSize = 1024; // bytes
    uint32_t maxPackets = 100;
    std::string interval = "100ms"; // Time::Parse format

    CommandLine cmd(__FILE__);
    cmd.AddValue("numNodes", "Number of nodes in the mesh network", numNodes);
    cmd.AddValue("simulationTime", "Total duration of the simulation (seconds)", simulationTime);
    cmd.AddValue("payloadSize", "Size of UDP client packet payload (bytes)", payloadSize);
    cmd.AddValue("maxPackets", "Maximum number of packets to send for UDP client", maxPackets);
    cmd.AddValue("interval", "Interval between packets for UDP client", interval);
    cmd.Parse(argc, argv);

    // Enable logging for specific components if needed
    // LogComponentEnable("WifiMeshAdHoc", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("AdhocWifiMac", LOG_LEVEL_INFO);
    // LogComponentEnable("RandomWalk2dMobilityModel", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Set up mobility for nodes
    MobilityHelper mobility;
    
    // Set initial positions using a grid allocator
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0), // Spacing between nodes
                                  "DeltaY", DoubleValue(20.0),
                                  "GridWidth", UintegerValue(numNodes), // For 5 nodes, arrange them in a row
                                  "LayoutType", StringValue("RowFirst"));
    
    // Configure random walk mobility
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-100, 100, -100, 100)), // Area for random walk
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=10.0]")); // Constant speed of 10 m/s
    mobility.Install(nodes);

    // Set up Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a); // Using 802.11a standard

    // Set up YansWifiPhy and YansWifiChannel
    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetChannel(YansWifiChannelHelper::Create());
    
    // Set a common transmit power for all nodes
    wifiPhy.Set("TxPowerStart", DoubleValue(16.0)); // dBm
    wifiPhy.Set("TxPowerEnd", DoubleValue(16.0));   // dBm

    // Set up MAC layer for Ad-Hoc mode with AarfWifiManager
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac",
                    "Ssid", SsidValue(Ssid("ns3-mesh-adhoc")),
                    "WifiManager", StringValue("ns3::AarfWifiManager")); // Use AarfWifiManager for rate control

    // Install Wi-Fi devices on nodes
    NetDeviceContainer devices;
    devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Install Internet stack on nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to Wi-Fi devices
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up UDP applications
    // UDP Server on the last node (index numNodes - 1)
    uint16_t port = 9; // Standard echo port
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(numNodes - 1));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    // UDP Client on the first node (index 0)
    // Client sends packets to the server's IP address and port
    UdpEchoClientHelper echoClient(interfaces.GetAddress(numNodes - 1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    echoClient.SetAttribute("Interval", TimeValue(Time::Parse(interval)));
    echoClient.SetAttribute("PacketSize", UintegerValue(payloadSize));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0)); // Start client slightly after server is up
    clientApps.Stop(Seconds(simulationTime));

    // Enable PCAP tracing for Wi-Fi devices
    wifiPhy.EnablePcapAll("wifi-mesh-adhoc");

    // Set simulation stop time
    Simulator::Stop(Seconds(simulationTime));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}