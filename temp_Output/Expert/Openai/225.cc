#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t numVehicles = 10;
    double simTime = 30.0;

    CommandLine cmd;
    cmd.AddValue("numVehicles", "Number of vehicle nodes", numVehicles);
    cmd.AddValue("simTime", "Simulation time in s", simTime);
    cmd.Parse(argc, argv);

    NodeContainer vehicles;
    vehicles.Create(numVehicles);

    NodeContainer rsu;
    rsu.Create(1);

    NodeContainer allNodes;
    allNodes.Add(vehicles);
    allNodes.Add(rsu);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211p);
    WifiMacHelper mac;
    QosWaveMacHelper waveMac = QosWaveMacHelper::Default();
    Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default();
    NetDeviceContainer devices;
    devices = wifi80211p.Install(phy, waveMac, allNodes);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(rsu);
    Ptr<MobilityModel> rsuMob = rsu.Get(0)->GetObject<MobilityModel>();
    rsuMob->SetPosition(Vector(500, 0, 0));

    WaypointMobilityModelHelper wmm;
    double highwayLength = 1000.0;
    double laneY = 10.0;
    double speed = 20.0; // m/s
    double startX = 0.0;
    double interval = highwayLength / numVehicles;
    for (uint32_t i = 0; i < numVehicles; ++i)
    {
        Ptr<Node> vehicle = vehicles.Get(i);
        Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
        posAlloc->Add(Vector(startX + i * interval, laneY, 0.0));

        Ptr<WaypointMobilityModel> mob = CreateObject<WaypointMobilityModel>();
        Waypoint wpStart(Seconds(0.0), Vector(startX + i * interval, laneY, 0.0));
        Waypoint wpEnd(Seconds(simTime), Vector(startX + i * interval + speed * simTime, laneY, 0.0));
        mob->AddWaypoint(wpStart);
        mob->AddWaypoint(wpEnd);
        vehicle->AggregateObject(mob);
    }

    InternetStackHelper stack;
    DsdvHelper dsdv;
    stack.SetRoutingHelper(dsdv);
    stack.Install(allNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.0.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t udpPort = 4000;
    ApplicationContainer serverApps;
    UdpServerHelper udpServer(udpPort);
    serverApps = udpServer.Install(rsu.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numVehicles; ++i)
    {
        UdpClientHelper udpClient(interfaces.GetAddress(numVehicles), udpPort);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        udpClient.SetAttribute("PacketSize", UintegerValue(512));
        clientApps.Add(udpClient.Install(vehicles.Get(i)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}