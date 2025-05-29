#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t nVehicles = 10;
    double simTime = 30.0;
    double highwayLength = 500.0;
    double laneY = 10.0;

    CommandLine cmd;
    cmd.AddValue("nVehicles", "Number of vehicle nodes", nVehicles);
    cmd.Parse(argc, argv);

    NodeContainer vehicleNodes;
    vehicleNodes.Create(nVehicles);

    NodeContainer rsuNode;
    rsuNode.Create(1);

    NodeContainer allNodes;
    allNodes.Add(vehicleNodes);
    allNodes.Add(rsuNode);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devs = wifi.Install(wifiPhy, wifiMac, allNodes);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator>();

    for (uint32_t i = 0; i < nVehicles; ++i)
    {
        pos->Add(Vector(0.0, laneY + 5.0 * i, 0.0));
    }
    // RSU at (highway center, separated from lane)
    pos->Add(Vector(highwayLength / 2, laneY + 20.0, 0.0));

    mobility.SetMobilityModel("ns3::WaypointMobilityModel");
    mobility.SetPositionAllocator(pos);
    mobility.Install(allNodes);

    // Waypoints for vehicles: move from (0, y) to (highwayLength, y) over simTime
    for (uint32_t i = 0; i < nVehicles; ++i)
    {
        Ptr<Node> node = vehicleNodes.Get(i);
        Ptr<WaypointMobilityModel> mob = node->GetObject<WaypointMobilityModel>();
        Vector start(0.0, laneY + 5.0 * i, 0.0);
        Vector end(highwayLength, laneY + 5.0 * i, 0.0);
        mob->AddWaypoint(Waypoint(Seconds(0.0), start));
        mob->AddWaypoint(Waypoint(Seconds(simTime), end));
    }
    // RSU stationary
    Ptr<Node> rsu = rsuNode.Get(0);
    Ptr<WaypointMobilityModel> rsuMob = rsu->GetObject<WaypointMobilityModel>();
    Vector rsuPos(highwayLength / 2, laneY + 20.0, 0.0);
    rsuMob->AddWaypoint(Waypoint(Seconds(0.0), rsuPos));
    rsuMob->AddWaypoint(Waypoint(Seconds(simTime), rsuPos));

    InternetStackHelper stack;
    DsdvHelper dsdv;
    stack.SetRoutingHelper(dsdv);
    stack.Install(allNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devs);

    uint16_t port = 4000;
    Address rsuAddress = Address(interfaces.GetAddress(nVehicles));
    ApplicationContainer sinkApps;
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(nVehicles), port));
    sinkApps.Add(sinkHelper.Install(rsuNode.Get(0)));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simTime + 1));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < nVehicles; ++i)
    {
        OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(nVehicles), port));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
        onoff.SetAttribute("PacketSize", UintegerValue(256));
        onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0 + 0.1 * i)));
        onoff.SetAttribute("StopTime", TimeValue(Seconds(simTime)));
        clientApps.Add(onoff.Install(vehicleNodes.Get(i)));
    }

    AsciiTraceHelper ascii;
    wifiPhy.EnableAsciiAll(ascii.CreateFileStream("vanet.tr"));

    Simulator::Stop(Seconds(simTime + 1));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}