#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMobilitySimulation");

int main(int argc, char* argv[])
{
    // 1. Set up simulation parameters
    double simulationTime = 120.0; // seconds
    std::string phyMode = "DsssRate1Mbps"; // Slower rate for better range visibility
    double rtsCtsThreshold = 2200; // Bytes
    double fragmentThreshold = 2200; // Bytes
    double apTxPower = 10.0; // dBm
    double staTxPower = 10.0; // dBm

    CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Total duration of the simulation in seconds", simulationTime);
    cmd.AddValue("phyMode", "Wifi Phy Mode", phyMode);
    cmd.AddValue("rtsCtsThreshold", "RTS/CTS Threshold", rtsCtsThreshold);
    cmd.AddValue("fragmentThreshold", "Fragment Threshold", fragmentThreshold);
    cmd.AddValue("apTxPower", "AP Transmit Power (dBm)", apTxPower);
    cmd.AddValue("staTxPower", "STA Transmit Power (dBm)", staTxPower);
    cmd.Parse(argc, argv);

    // Configure Wifi standard for all devices
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
    wifi.SetRemoteStationManager(
        "ns3::AarfWifiManager", "DisableRtsCts", BooleanValue(false), "FragmentationThreshold", UintegerValue(fragmentThreshold));

    // 2. Create Nodes
    NodeContainer apNodes;
    apNodes.Create(3); // Three APs
    NodeContainer staNodes;
    staNodes.Create(6); // Six STAs
    Ptr<Node> serverNode = CreateObject<Node>(); // One remote server node

    // 3. Create YansWifiChannel and YansWifiPhy
    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel", "Frequency", DoubleValue(2.4e9));

    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.Set("TxPower", DoubleValue(apTxPower)); // Default AP power

    // 4. Configure MAC for APs and STAs
    // AP MAC configuration
    WifiMacHelper apMac;
    apMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("ns3-ap")), "BeaconInterval", TimeValue(MicroSeconds(102400)));
    NetDeviceContainer apDevices = phy.Install(apMac, apNodes);

    // STA MAC configuration
    phy.Set("TxPower", DoubleValue(staTxPower)); // Default STA power for consistency
    WifiMacHelper staMac;
    staMac.SetType("ns3::ActiveProbingWifiMac", // Required for active scanning and handover
                   "Ssid", SsidValue(Ssid("ns3-ap")),
                   "ActiveProbing", BooleanValue(true)); // Explicitly enable active probing
    NetDeviceContainer staDevices = phy.Install(staMac, staNodes);

    // 5. Install Internet Stack
    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);
    stack.Install(serverNode);

    // 6. Setup CSMA network for APs and Server (Backbone)
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer csmaDevices;
    for (uint32_t i = 0; i < apNodes.GetN(); ++i)
    {
        csmaDevices.Add(csma.Install(apNodes.Get(i)));
    }
    csmaDevices.Add(csma.Install(serverNode));

    // 7. Assign IP Addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer apIpInterfaces;
    apIpInterfaces = ipv4.Assign(apDevices);

    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staIpInterfaces;
    staIpInterfaces = ipv4.Assign(staDevices);

    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaIpInterfaces;
    csmaIpInterfaces = ipv4.Assign(csmaDevices);

    // Populate routing tables (essential for traffic flow between subnets)
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // 8. Mobility Model for APs and STAs
    MobilityHelper mobility;

    // Fixed positions for APs
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    Ptr<ListPositionAllocator> apPositionAlloc = CreateObject<ListPositionAllocator>();
    apPositionAlloc->Add(Vector(0.0, 0.0, 0.0));   // AP0
    apPositionAlloc->Add(Vector(50.0, 0.0, 0.0));  // AP1
    apPositionAlloc->Add(Vector(100.0, 0.0, 0.0)); // AP2
    mobility.SetPositionAllocator(apPositionAlloc);
    mobility.Install(apNodes);

    // Fixed position for Server
    mobility.SetPositionAllocator(CreateObject<ConstantPositionAllocator>(Vector(50.0, 50.0, 0.0)));
    mobility.Install(serverNode);

    // Waypoint mobility for STAs to simulate movement between APs
    mobility.SetMobilityModel("ns3::WaypointMobilityModel");
    for (uint32_t i = 0; i < staNodes.GetN(); ++i)
    {
        Ptr<WaypointMobilityModel> wpmm = CreateObject<WaypointMobilityModel>();
        
        // Initial positions and waypoints for STAs to move between AP zones
        if (i < 2) { // STAs 0, 1 start near AP0 (0,0,0)
            wpmm->AddWaypoint(Waypoint(Seconds(0.0), Vector(5.0 + i, 5.0, 0.0)));
            wpmm->AddWaypoint(Waypoint(Seconds(simulationTime * 0.3), Vector(55.0 + i, 5.0, 0.0))); // Move towards AP1
            wpmm->AddWaypoint(Waypoint(Seconds(simulationTime * 0.6), Vector(105.0 + i, 5.0, 0.0))); // Move towards AP2
            wpmm->AddWaypoint(Waypoint(Seconds(simulationTime * 0.9), Vector(55.0 + i, 5.0, 0.0))); // Move back towards AP1
        } else if (i < 4) { // STAs 2, 3 start near AP1 (50,0,0)
            wpmm->AddWaypoint(Waypoint(Seconds(0.0), Vector(45.0 + (i - 2), 5.0, 0.0)));
            wpmm->AddWaypoint(Waypoint(Seconds(simulationTime * 0.3), Vector(5.0 + (i - 2), 5.0, 0.0))); // Move towards AP0
            wpmm->AddWaypoint(Waypoint(Seconds(simulationTime * 0.6), Vector(105.0 + (i - 2), 5.0, 0.0))); // Move towards AP2
            wpmm->AddWaypoint(Waypoint(Seconds(simulationTime * 0.9), Vector(45.0 + (i - 2), 5.0, 0.0))); // Move back towards AP1
        } else { // STAs 4, 5 start near AP2 (100,0,0)
            wpmm->AddWaypoint(Waypoint(Seconds(0.0), Vector(95.0 + (i - 4), 5.0, 0.0)));
            wpmm->AddWaypoint(Waypoint(Seconds(simulationTime * 0.3), Vector(45.0 + (i - 4), 5.0, 0.0))); // Move towards AP1
            wpmm->AddWaypoint(Waypoint(Seconds(simulationTime * 0.6), Vector(5.0 + (i - 4), 5.0, 0.0))); // Move towards AP0
            wpmm->AddWaypoint(Waypoint(Seconds(simulationTime * 0.9), Vector(95.0 + (i - 4), 5.0, 0.0))); // Move back towards AP2
        }
        staNodes.Get(i)->AggregateObject(wpmm);
    }
    mobility.Install(staNodes);

    // 9. Setup UDP Traffic
    uint16_t port = 9; // Discard port for packet sink
    // The server's IP address on the CSMA network is the last assigned IP for the csmaDevices container
    // which consists of APs (0,1,2) and the server (3). So it's csmaIpInterfaces.GetAddress(3).
    Address serverAddress = InetSocketAddress(csmaIpInterfaces.GetAddress(3), port); 

    // Packet Sink (Server side)
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", serverAddress);
    ApplicationContainer serverApps = sinkHelper.Install(serverNode);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    // OnOff Application (Client/STA side)
    OnOffHelper onoff("ns3::UdpSocketFactory", Address());
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    onoff.SetAttribute("DataRate", StringValue("1Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < staNodes.GetN(); ++i)
    {
        Ptr<Node> staNode = staNodes.Get(i);
        AddressValue remoteAddress(InetSocketAddress(csmaIpInterfaces.GetAddress(3), port));
        onoff.SetAttribute("Remote", remoteAddress);
        clientApps.Add(onoff.Install(staNode));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime - 1.0));

    // 10. NetAnim Visualization
    NetAnimHelper anim;
    anim.SetTraceFile("wifi-mobility-netanim.xml");
    anim.EnablePacketMetadata(true); // Shows packet flow
    anim.EnableIpv4RouteTracking(true); // Shows routing table changes
    anim.EnableWifiMacTracking(apDevices, staDevices); // Important for visualizing associations
    anim.EnableMobilityTracking(); // Track node movement
    anim.TrackReconfiguredAccessPoints(); // Tracks AP reconfigurations (useful for handovers)

    // 11. Run Simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}