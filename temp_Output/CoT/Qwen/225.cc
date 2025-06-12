#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/dsdv-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetUdpDsdvSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: 5 vehicles + 1 RSU
    NodeContainer vehicles;
    vehicles.Create(5);
    NodeContainer rsuNode;
    rsuNode.Create(1);

    // Setup mobility for highway scenario using WaypointMobilityModel
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::WaypointMobilityModel");
    mobility.Install(vehicles);

    // Add waypoints for movement along the highway
    for (auto i = vehicles.Begin(); i != vehicles.End(); ++i) {
        Ptr<Node> node = *i;
        Ptr<MobilityModel> mobModel = node->GetObject<MobilityModel>();
        auto wayMobModel = DynamicCast<WaypointMobilityModel>(mobModel);
        if (wayMobModel) {
            wayMobModel->AddWaypoint(Waypoint(Seconds(0), Vector(0, 0, 0)));
            wayMobModel->AddWaypoint(Waypoint(Seconds(30), Vector(1000, 0, 0)));
        }
    }

    // Set position for RSU at the center of the highway
    Ptr<ListPositionAllocator> rsuPositionAlloc = CreateObject<ListPositionAllocator>();
    rsuPositionAlloc->Add(Vector(500, 50, 0)); // Off the road
    mobility.SetPositionAllocator(rsuPositionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(rsuNode);

    // Configure WiFi for VANET communication
    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211p);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("OfdmRate6Mbps"),
                                 "ControlMode", StringValue("OfdmRate6Mbps"));

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer vehicleDevices = wifi.Install(wifiPhy, wifiMac, vehicles);
    NetDeviceContainer rsuDevice = wifi.Install(wifiPhy, wifiMac, rsuNode);

    // Install internet stack with DSDV routing protocol
    InternetStackHelper stack;
    DsdvHelper dsdv;
    stack.SetRoutingHelper(dsdv);
    stack.Install(vehicles);
    stack.Install(rsuNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer vehicleInterfaces = address.Assign(vehicleDevices);
    Ipv4InterfaceContainer rsuInterface = address.Assign(rsuDevice);

    // UDP echo server on RSU to receive packets
    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApp = server.Install(rsuNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(30.0));

    // Periodic UDP traffic from each vehicle to RSU
    uint32_t packetSize = 1024;
    Time interPacketInterval = Seconds(1.0);
    for (uint32_t i = 0; i < vehicles.GetN(); ++i) {
        UdpEchoClientHelper client(rsuInterface.GetAddress(0), port);
        client.SetAttribute("MaxPackets", UintegerValue(30));
        client.SetAttribute("Interval", TimeValue(interPacketInterval));
        client.SetAttribute("PacketSize", UintegerValue(packetSize));

        ApplicationContainer clientApp = client.Install(vehicles.Get(i));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(30.0));
    }

    // Enable PCAP tracing
    wifiPhy.EnablePcap("vanet_udp_dsdv_simulation", vehicleDevices);
    wifiPhy.EnablePcap("vanet_udp_dsdv_simulation_rsu", rsuDevice);

    // Simulation duration
    Simulator::Stop(Seconds(30.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}