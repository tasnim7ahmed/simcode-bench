#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-apps-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UavAodvSimulation");

int main(int argc, char *argv[])
{
    // Simulation parameters
    uint32_t numUavs = 5;
    double simTime = 60.0;

    CommandLine cmd;
    cmd.AddValue("numUavs", "Number of UAV nodes", numUavs);
    cmd.Parse(argc, argv);

    // Create nodes: UAVs + 1 GCS
    NodeContainer nodes;
    nodes.Create(numUavs + 1); // last node is GCS

    // Set up wireless PHY and channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    WifiMacHelper mac;
    Ssid ssid = Ssid("uav-network");
    mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // 3D mobility: UAVs use RandomWaypointMobilityModel in 3D; GCS is stationary
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < numUavs; ++i)
    {
        positionAlloc->Add(Vector(100.0*i, 100.0, 100.0 + 10*i));
    }
    // GCS position
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);

    // UAVs mobility
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
        "Speed", StringValue("ns3::UniformRandomVariable[Min=5.0|Max=15.0]"),
        "Pause", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
        "PositionAllocator", PointerValue(positionAlloc),
        "MinX", DoubleValue(-500.0),
        "MinY", DoubleValue(-500.0),
        "MinZ", DoubleValue(50.0),
        "MaxX", DoubleValue(500.0),
        "MaxY", DoubleValue(500.0),
        "MaxZ", DoubleValue(200.0)
    );
    for (uint32_t i = 0; i < numUavs; ++i)
    {
        mobility.Install(nodes.Get(i));
    }
    // GCS stationary
    MobilityHelper gcsMobility;
    gcsMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    gcsMobility.Install(nodes.Get(numUavs));

    // Install Internet stack with AODV
    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv);
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP communication: Each UAV sends UDP packets to GCS
    uint16_t port = 5000;
    // GCS: install a UDP server
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(numUavs));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simTime));

    // UAVs: install UDP client applications (one per UAV)
    for (uint32_t i = 0; i < numUavs; ++i)
    {
        UdpClientHelper client(interfaces.GetAddress(numUavs), port);
        client.SetAttribute("MaxPackets", UintegerValue(120));
        client.SetAttribute("Interval", TimeValue(Seconds(0.5)));
        client.SetAttribute("PacketSize", UintegerValue(512));
        ApplicationContainer clientApp = client.Install(nodes.Get(i));
        clientApp.Start(Seconds(2.0 + i));
        clientApp.Stop(Seconds(simTime));
    }

    // Enable logging of reception at the server
    Config::SetDefault("ns3::UdpServer::PacketWindowSize", UintegerValue(1000));
    // Uncomment to enable detailed logging if desired:
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("AodvRoutingProtocol", LOG_LEVEL_INFO);

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}