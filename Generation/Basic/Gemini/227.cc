#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/aodv-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t nUAVs = 3;
    double simulationTime = 60.0;

    NodeContainer gcsNode;
    gcsNode.Create(1);

    NodeContainer uavNodes;
    uavNodes.Create(nUAVs);

    NodeContainer allNodes;
    allNodes.Add(gcsNode);
    allNodes.Add(uavNodes);

    MobilityHelper mobility;

    Ptr<ListPositionAllocator> positionAllocGCS = CreateObject<ListPositionAllocator>();
    positionAllocGCS->Add(Vector(0.0, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAllocGCS);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gcsNode);

    Ptr<ListPositionAllocator> positionAllocUAVs = CreateObject<ListPositionAllocator>();
    positionAllocUAVs->Add(Vector(100.0, 0.0, 10.0));
    positionAllocUAVs->Add(Vector(0.0, 100.0, 15.0));
    positionAllocUAVs->Add(Vector(-100.0, 0.0, 20.0));

    mobility.SetPositionAllocator(positionAllocUAVs);
    mobility.SetMobilityModel("ns3::WaypointMobilityModel");
    mobility.Install(uavNodes);

    Ptr<WaypointMobilityModel> wpm_uav1 = uavNodes.Get(0)->GetObject<WaypointMobilityModel>();
    wpm_uav1->AddWaypoint(Waypoint(Seconds(5.0), Vector(150.0, 50.0, 20.0)));
    wpm_uav1->AddWaypoint(Waypoint(Seconds(20.0), Vector(200.0, 0.0, 30.0)));
    wpm_uav1->AddWaypoint(Waypoint(Seconds(40.0), Vector(150.0, -50.0, 25.0)));
    wpm_uav1->AddWaypoint(Waypoint(Seconds(55.0), Vector(100.0, 0.0, 10.0)));

    Ptr<WaypointMobilityModel> wpm_uav2 = uavNodes.Get(1)->GetObject<WaypointMobilityModel>();
    wpm_uav2->AddWaypoint(Waypoint(Seconds(7.0), Vector(50.0, 150.0, 25.0)));
    wpm_uav2->AddWaypoint(Waypoint(Seconds(22.0), Vector(0.0, 200.0, 35.0)));
    wpm_uav2->AddWaypoint(Waypoint(Seconds(42.0), Vector(-50.0, 150.0, 30.0)));
    wpm_uav2->AddWaypoint(Waypoint(Seconds(57.0), Vector(0.0, 100.0, 15.0)));

    Ptr<WaypointMobilityModel> wpm_uav3 = uavNodes.Get(2)->GetObject<WaypointMobilityModel>();
    wpm_uav3->AddWaypoint(Waypoint(Seconds(9.0), Vector(-150.0, -50.0, 30.0)));
    wpm_uav3->AddWaypoint(Waypoint(Seconds(24.0), Vector(-200.0, 0.0, 40.0)));
    wpm_uav3->AddWaypoint(Waypoint(Seconds(44.0), Vector(-150.0, 50.0, 35.0)));
    wpm_uav3->AddWaypoint(Waypoint(Seconds(59.0), Vector(-100.0, 0.0, 20.0)));

    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    YansWifiPhyHelper phy;
    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel", "Frequency", DoubleValue(5.8e9));
    phy.SetChannel(channel.Create());

    NetDeviceContainer devices;
    devices = wifi.Install(phy, mac, allNodes);

    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(allNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    Ipv4Address gcsIp = interfaces.GetAddress(0);

    uint16_t port = 9;

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(gcsNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));

    UdpClientHelper client(gcsIp, port);
    client.SetAttribute("MaxPackets", UintegerValue(100));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < nUAVs; ++i)
    {
        clientApps.Add(client.Install(uavNodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}