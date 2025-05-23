#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

// Define NS3_LOG_COMPONENT_DEFINE to enable logging for this script
// NS3_LOG_COMPONENT_DEFINE("ManetTwoNodesUdp"); // Omitted as per "no commentary" rule

int
main(int argc, char* argv[])
{
    // Enable logging (optional, not strictly required by problem, but good practice)
    // LogComponentEnable("ManetTwoNodesUdp", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    // LogComponentEnable("RandomWaypointMobilityModel", LOG_LEVEL_INFO);

    // 1. Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // 2. Configure Wifi physical layer
    WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_PHY_STANDARD_80211b); // Simple standard for basic operation

    YansWifiPhyHelper phy;
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    YansWifiChannelHelper channel;
    channel.SetPropagationLoss("ns3::FriisPropagationLossModel");
    channel.SetPropagationDelay("ns3::ConstantPropagationDelayModel");
    phy.SetChannel(channel.Create());

    // 3. Configure MAC layer for Ad-Hoc
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices;
    devices = wifi.Install(phy, mac, nodes);

    // 4. Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // 5. Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // 6. Configure mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed",
                              StringValue("ns3::ConstantRandomVariable[Constant=20.0]"), // 20 m/s
                              "Pause",
                              StringValue("ns3::ConstantRandomVariable[Constant=0.0]")); // No pause time

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    // Initial positions (within 100m x 100m area for RandomWaypoint to start)
    positionAlloc->Add(Vector(10.0, 10.0, 0.0));
    positionAlloc->Add(Vector(90.0, 90.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);

    mobility.Install(nodes);

    // RandomWaypoint will define its bounds if not specified via PositionAllocator in specific ways
    // For a 100x100 area, we set the bounds on the model directly if not using a specific allocator
    // If using a ListPositionAllocator as above, RandomWaypoint assumes these are just starting points
    // and subsequent waypoints will be within the bounds of that allocator.
    // For explicit bounds:
    Ptr<RandomWaypointMobilityModel> rwmm0 = DynamicCast<RandomWaypointMobilityModel>(nodes.Get(0)->GetObject<MobilityModel>());
    rwmm0->SetPositionAllocator(
        CreateObject<RandomRectanglePositionAllocator>(
            ns3::Rectangle(0.0, 100.0, 0.0, 100.0)));

    Ptr<RandomWaypointMobilityModel> rwmm1 = DynamicCast<RandomWaypointMobilityModel>(nodes.Get(1)->GetObject<MobilityModel>());
    rwmm1->SetPositionAllocator(
        CreateObject<RandomRectanglePositionAllocator>(
            ns3::Rectangle(0.0, 100.0, 0.0, 100.0)));


    // 7. Configure UDP applications
    uint16_t port = 9; // Server port

    // Server (Node 0)
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApps = sinkHelper.Install(nodes.Get(0));
    serverApps.Start(Seconds(1.0)); // Server starts at 1 second
    serverApps.Stop(Seconds(10.0));

    // Client (Node 1)
    UdpClientHelper clientHelper(interfaces.GetAddress(0), port); // Server's IP address
    clientHelper.SetAttribute("MaxPackets", UintegerValue(1000));
    clientHelper.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = clientHelper.Install(nodes.Get(1));
    clientApps.Start(Seconds(2.0)); // Client starts at 2 seconds
    clientApps.Stop(Seconds(10.0)); // Client stops when simulation ends

    // 8. Set simulation time
    Simulator::Stop(Seconds(10.0));

    // 9. Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}