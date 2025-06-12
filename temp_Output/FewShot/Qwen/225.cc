#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/dsdv-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set simulation time
    double simTime = 30.0;
    uint32_t numVehicles = 5;
    uint16_t port = 4000;

    // Enable logging for DSDV
    LogComponentEnable("DsdvRoutingProtocol", LOG_LEVEL_INFO);

    // Create nodes (vehicles + RSU)
    NodeContainer vehicles;
    vehicles.Create(numVehicles);

    NodeContainer rsuNode;
    rsuNode.Create(1);

    NodeContainer allNodes = NodeContainer(vehicles, rsuNode);

    // Setup mobility for highway scenario
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::WaypointMobilityModel");
    mobility.Install(allNodes);

    // Assign positions to RSU (stationary)
    Ptr<MobilityModel> rsuMobility = rsuNode.Get(0)->GetObject<MobilityModel>();
    rsuMobility->SetPosition(Vector(500.0, 0.0, 0.0));

    // Add waypoints for vehicles moving along the highway
    for (uint32_t i = 0; i < numVehicles; ++i) {
        Ptr<Node> node = vehicles.Get(i);
        Ptr<WaypointMobilityModel> waypointMobility = node->GetObject<WaypointMobilityModel>();

        waypointMobility->AddWaypoint(Waypoint(Seconds(0.0), Vector(i * 100.0, 0.0, 0.0)));
        waypointMobility->AddWaypoint(Waypoint(Seconds(simTime), Vector((i * 100.0) + 1000.0, 0.0, 0.0)));
    }

    // Configure WiFi
    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211p);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager");

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, allNodes);

    // Install Internet stack with DSDV routing
    DsdvHelper dsdv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(dsdv);
    stack.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Get RSU IP address
    Ipv4Address rsuIp = interfaces.GetAddress(1, 0); // assuming RSU is second node

    // Install UDP packet sink on RSU
    PacketSinkHelper packetSink("ns3::UdpSocketFactory", InetSocketAddress(rsuIp, port));
    ApplicationContainer sinkApp = packetSink.Install(rsuNode);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simTime));

    // Install OnOff UdpClient applications on each vehicle
    for (uint32_t i = 0; i < numVehicles; ++i) {
        Ipv4Address vehicleIp = interfaces.GetAddress(i, 0);

        OnOffHelper client("ns3::UdpSocketFactory", InetSocketAddress(rsuIp, port));
        client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        client.SetAttribute("DataRate", DataRateValue(DataRate("100kbps")));
        client.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer app = client.Install(vehicles.Get(i));
        app.Start(Seconds(1.0));
        app.Stop(Seconds(simTime));
    }

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}