#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"

int main(int argc, char *argv[])
{
    // Enable logging for UDP Echo applications
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("WifiPhy", LOG_LEVEL_INFO); // Optional: for Wi-Fi physical layer logging

    // Set default simulation time to 10 seconds
    double simulationTime = 10.0;
    
    CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Total duration of the simulation in seconds", simulationTime);
    cmd.Parse(argc, argv);

    // Create two nodes: one client and one AP
    NodeContainer nodes;
    nodes.Create(2);

    Ptr<Node> apNode = nodes.Get(0);
    Ptr<Node> clientNode = nodes.Get(1);

    // Install Internet stack on both nodes
    InternetStackHelper internet;
    internet.Install(nodes);

    // Create Wi-Fi channel and PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // Configure Wi-Fi MACs and standard
    WifiHelper wifi;
    // Set 802.11n standard (using 5GHz band as an example)
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ); 

    Ssid ssid = Ssid("ns3-wifi-ap");

    // Configure AP MAC
    NqosWifiMacHelper apMac = NqosWifiMacHelper::Default();
    apMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, apMac, apNode);

    // Configure STA MAC
    NqosWifiMacHelper staMac = NqosWifiMacHelper::Default();
    staMac.SetType("ns3::StaWifiMac",
                   "Ssid", SsidValue(ssid),
                   "ActiveProbing", BooleanValue(false)); // STA doesn't need to probe if AP is already broadcasting
    NetDeviceContainer clientDevice = wifi.Install(phy, staMac, clientNode);

    // Configure mobility for nodes (static positions)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0), // AP at (0,0), Client at (5,0)
                                  "DeltaY", DoubleValue(0.0),
                                  "LayoutType", StringValue("ns3::GridPositionAllocator::ROW_FIRST"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apIpInterface = ipv4.Assign(apDevice);
    Ipv4InterfaceContainer clientIpInterface = ipv4.Assign(clientDevice);

    // Get the AP's IP address for the client to send to
    Ipv4Address apAddress = apIpInterface.GetAddress(0);

    // Setup UDP Echo Server on AP
    uint16_t port = 9; // Echo server listens on port 9
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(apNode);
    serverApps.Start(Seconds(1.0)); // Server starts at 1 second
    serverApps.Stop(Seconds(simulationTime));

    // Setup UDP Echo Client on client
    UdpEchoClientHelper echoClient(apAddress, port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10)); // Send 10 packets
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // 1 packet per second
    echoClient.SetAttribute("PacketSize", UintegerValue(1024)); // 1024 bytes per packet
    ApplicationContainer clientApps = echoClient.Install(clientNode);
    clientApps.Start(Seconds(2.0)); // Client starts at 2 seconds (after server)
    clientApps.Stop(Seconds(simulationTime - 1.0)); // Client stops 1 second before end

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}