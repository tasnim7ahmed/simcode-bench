#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/ocb-wifi-mac.h"
#include "ns3/constant-position-mobility-model.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Simulation parameters
    uint32_t numVehicles = 10;
    double simulationTime = 30.0;
    double packetInterval = 1.0; // Send a packet every second

    // Create nodes
    NodeContainer vehicles;
    vehicles.Create(numVehicles);

    NodeContainer rsu;
    rsu.Create(1);

    // Install Internet stack
    InternetStackHelper internet;
    internet.SetRoutingHelper(DsdvHelper());
    internet.Install(vehicles);
    internet.Install(rsu);

    // Configure mobility model for vehicles
    MobilityHelper mobilityVehicles;
    mobilityVehicles.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                            "X", StringValue("100.0"),
                                            "Y", StringValue("100.0"),
                                            "Z", StringValue("0.0"),
                                            "Rho", StringValue("ns3::UniformRandomVariable[Min=0|Max=50]"));
    mobilityVehicles.SetMobilityModel("ns3::WaypointMobilityModel");
    mobilityVehicles.Install(vehicles);

    for (uint32_t i = 0; i < numVehicles; ++i) {
        Ptr<WaypointMobilityModel> mobility = vehicles.Get(i)->GetObject<WaypointMobilityModel>();
        mobility->AddWaypoint(Waypoint{Seconds(0), Vector3D(100 + (i * 10), 100, 0)});
        mobility->AddWaypoint(Waypoint{Seconds(simulationTime), Vector3D(100 + (i * 10), 200, 0)});
    }

    // Configure mobility model for RSU (static)
    MobilityHelper mobilityRsu;
    mobilityRsu.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityRsu.Install(rsu);
    rsu.Get(0)->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector3D(150, 150, 0));

    // Configure 802.11p (WAVE)
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    NqosWaveMacHelper wifiMac = NqosWaveMacHelper::Default();
    Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default();

    NetDeviceContainer vehicleDevices = wifi80211p.Install(wifiPhy, wifiMac, vehicles);
    NetDeviceContainer rsuDevices = wifi80211p.Install(wifiPhy, wifiMac, rsu);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer vehicleInterfaces = address.Assign(vehicleDevices);
    Ipv4InterfaceContainer rsuInterfaces = address.Assign(rsuDevices);

    // UDP Client (Vehicles)
    UdpClientHelper client(rsuInterfaces.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(static_cast<uint32_t>(simulationTime / packetInterval)));
    client.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numVehicles; ++i) {
        clientApps.Add(client.Install(vehicles.Get(i)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime));

    // UDP Server (RSU)
    UdpServerHelper server(9);
    ApplicationContainer serverApp = server.Install(rsu.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simulationTime));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}