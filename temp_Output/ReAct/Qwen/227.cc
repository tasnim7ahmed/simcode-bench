#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UAVDroneSimulation");

int main(int argc, char *argv[]) {
    uint32_t numUavs = 5;
    double simTime = 60.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numUavs", "Number of UAV nodes", numUavs);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.Parse(argc, argv);

    // Enable AODV logging
    LogComponentEnable("AodvRoutingProtocol", LOG_LEVEL_INFO);

    // Create nodes: UAVs + GCS
    NodeContainer uavNodes;
    uavNodes.Create(numUavs);

    NodeContainer gcsNode;
    gcsNode.Create(1);

    // Setup WiFi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("OfdmRate18Mbps"),
                                 "ControlMode", StringValue("OfdmRate6Mbps"));

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    channel.AddPropagationLoss("ns3::ThreeLogDistancePropagationLossModel");
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");

    WifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices;
    devices = wifi.Install(phy, mac, uavNodes);
    devices.Add(wifi.Install(phy, mac, gcsNode.Get(0)));

    // Setup Internet stack with AODV
    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv);
    stack.Install(uavNodes);
    stack.Install(gcsNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Mobility model - 3D for UAVs and static for GCS
    MobilityHelper mobilityUav;
    mobilityUav.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                                 "Speed", StringValue("ns3::UniformRandomVariable[Min=5.0|Max=15.0]"),
                                 "Pause", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                                 "PositionAllocator", CreateObject<RandomBoxPositionAllocator>());
    mobilityUav.Install(uavNodes);

    MobilityHelper mobilityGcs;
    mobilityGcs.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityGcs.Install(gcsNode);

    // Setup UDP echo server on GCS
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(gcsNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    // Setup UDP echo clients on UAVs
    uint32_t packetSize = 1024;
    Time interPacketInterval = Seconds(1.0);
    uint32_t maxPacketCount = static_cast<uint32_t>(simTime / interPacketInterval.GetSeconds());

    for (uint32_t i = 0; i < numUavs; ++i) {
        UdpEchoClientHelper echoClient(interfaces.GetAddress(numUavs), 9); // GCS IP is last assigned
        echoClient.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
        echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
        echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
        ApplicationContainer clientApps = echoClient.Install(uavNodes.Get(i));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(simTime));
    }

    // Set simulation stop time
    Simulator::Stop(Seconds(simTime));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}