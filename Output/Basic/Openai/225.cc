#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/dsdv-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t numVehicles = 10;
    double highwayLength = 500.0;
    double highwayWidth = 20.0;
    double vehicleSpeed = 20.0; // m/s
    uint16_t udpPort = 4000;
    double simTime = 30.0;

    CommandLine cmd;
    cmd.AddValue("numVehicles", "Number of VANET vehicles", numVehicles);
    cmd.AddValue("simTime", "Simulation time (seconds)", simTime);
    cmd.Parse(argc, argv);

    NodeContainer vehicles;
    vehicles.Create(numVehicles);
    Ptr<Node> rsu = CreateObject<Node>();

    NodeContainer allNodes;
    allNodes.Add(vehicles);
    allNodes.Add(rsu);

    // Wifi physical and MAC
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211p);

    NqosWaveMacHelper wifiMac = NqosWaveMacHelper::Default();

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, allNodes);

    // Mobility: WaypointMobilityModel for vehicles, constant position for RSU
    MobilityHelper mobilityVehicles, mobilityRSU;

    mobilityVehicles.SetMobilityModel("ns3::WaypointMobilityModel");
    mobilityVehicles.Install(vehicles);

    for (uint32_t i = 0; i < numVehicles; ++i)
    {
        Ptr<Node> n = vehicles.Get(i);
        Ptr<WaypointMobilityModel> mob = n->GetObject<WaypointMobilityModel>();
        double startPos = 10.0 + i * (highwayLength - 20.0) / (numVehicles - 1);
        double y = highwayWidth / 2.0;
        Vector pos = Vector(startPos, y, 0.0);
        mob->AddWaypoint(Waypoint(Seconds(0.0), pos));
        Vector endPos = Vector(startPos + vehicleSpeed * simTime, y, 0.0);
        if (endPos.x > highwayLength - 10.0) endPos.x = highwayLength - 10.0;
        mob->AddWaypoint(Waypoint(Seconds(simTime), endPos));
    }

    Ptr<ListPositionAllocator> rsuPos = CreateObject<ListPositionAllocator>();
    rsuPos->Add(Vector(highwayLength / 2.0, highwayWidth + 20.0, 0.0)); // RSU at side of the road
    mobilityRSU.SetPositionAllocator(rsuPos);
    mobilityRSU.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityRSU.Install(rsu);

    // Internet stack (DSDV Routing)
    DsdvHelper dsdv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(dsdv);
    internet.Install(allNodes);

    // IP addressing
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP server on RSU
    UdpServerHelper udpServer(udpPort);
    ApplicationContainer serverApps = udpServer.Install(rsu);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    // UDP clients on vehicles
    for (uint32_t i = 0; i < numVehicles; ++i)
    {
        UdpClientHelper udpClient(interfaces.GetAddress(numVehicles), udpPort);
        udpClient.SetAttribute("MaxPackets", UintegerValue(320));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        udpClient.SetAttribute("PacketSize", UintegerValue(64));
        ApplicationContainer clientApps = udpClient.Install(vehicles.Get(i));
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(simTime));
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}