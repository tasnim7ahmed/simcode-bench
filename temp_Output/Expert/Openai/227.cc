#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t nDrones = 5;
    double simTime = 60.0;
    double droneAltMin = 80.0;
    double droneAltMax = 120.0;
    double areaX = 500.0;
    double areaY = 500.0;

    CommandLine cmd;
    cmd.AddValue("nDrones", "Number of UAVs", nDrones);
    cmd.Parse(argc, argv);

    NodeContainer drones;
    drones.Create(nDrones);

    NodeContainer gcs;
    gcs.Create(1);

    NodeContainer allNodes;
    allNodes.Add(gcs);
    allNodes.Add(drones);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, allNodes);

    MobilityHelper mobility;

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(areaX / 2, areaY / 2, 1.0)); // GCS position (on ground)

    // Drones initial positions
    Ptr<UniformRandomVariable> randX = CreateObject<UniformRandomVariable>();
    randX->SetAttribute("Min", DoubleValue(0.0));
    randX->SetAttribute("Max", DoubleValue(areaX));
    Ptr<UniformRandomVariable> randY = CreateObject<UniformRandomVariable>();
    randY->SetAttribute("Min", DoubleValue(0.0));
    randY->SetAttribute("Max", DoubleValue(areaY));
    Ptr<UniformRandomVariable> randZ = CreateObject<UniformRandomVariable>();
    randZ->SetAttribute("Min", DoubleValue(droneAltMin));
    randZ->SetAttribute("Max", DoubleValue(droneAltMax));

    for (uint32_t i = 0; i < nDrones; ++i)
    {
        positionAlloc->Add(Vector(randX->GetValue(), randY->GetValue(), randZ->GetValue()));
    }

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
        "Speed", StringValue("ns3::UniformRandomVariable[Min=10.0|Max=15.0]"),
        "Pause", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
        "PositionAllocator", PointerValue(positionAlloc),
        "ZBounds", StringValue(std::to_string(droneAltMin) + "|" + std::to_string(droneAltMax)));
    mobility.Install(drones);

    MobilityHelper gcsMobility;
    gcsMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    gcsMobility.Install(gcs);

    InternetStackHelper internet;
    AodvHelper aodv;
    internet.SetRoutingHelper(aodv);
    internet.Install(allNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.0.0", "255.255.0.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Application Setup: each drone sends to GCS using UDP,
    // and GCS responds back to the first drone
    uint16_t droneToGcsPort = 9000;
    uint16_t gcsToDronePort = 9001;

    // GCS UDP Sink (receives from drones)
    UdpServerHelper gcsServer(droneToGcsPort);
    ApplicationContainer gcsServerApps = gcsServer.Install(gcs.Get(0));
    gcsServerApps.Start(Seconds(0.0));
    gcsServerApps.Stop(Seconds(simTime));

    // Ground station will send reply packets only to UAV 0 for demo
    UdpServerHelper droneServer(gcsToDronePort);
    ApplicationContainer droneServerApps = droneServer.Install(drones.Get(0));
    droneServerApps.Start(Seconds(0.0));
    droneServerApps.Stop(Seconds(simTime));

    // Drones' UDP Client: send packets to GCS
    for (uint32_t i = 0; i < nDrones; ++i)
    {
        UdpClientHelper client(interfaces.GetAddress(0), droneToGcsPort);
        client.SetAttribute("MaxPackets", UintegerValue(10000));
        client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        client.SetAttribute("PacketSize", UintegerValue(64));
        ApplicationContainer clientApp = client.Install(drones.Get(i));
        clientApp.Start(Seconds(1.0 + i * 0.2));
        clientApp.Stop(Seconds(simTime - 5.0));
    }

    // GCS sends reply to drone 0
    UdpClientHelper replyClient(interfaces.GetAddress(1), gcsToDronePort);
    replyClient.SetAttribute("MaxPackets", UintegerValue(8000));
    replyClient.SetAttribute("Interval", TimeValue(Seconds(1.2)));
    replyClient.SetAttribute("PacketSize", UintegerValue(48));
    ApplicationContainer replyApp = replyClient.Install(gcs.Get(0));
    replyApp.Start(Seconds(2.0));
    replyApp.Stop(Seconds(simTime - 2.0));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}