#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/dsdv-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VANETDSDVSimulation");

int main(int argc, char *argv[]) {
    uint32_t numVehicles = 5;
    double simDuration = 30.0;
    uint16_t udpPort = 9;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numVehicles", "Number of vehicle nodes", numVehicles);
    cmd.Parse(argc, argv);

    NodeContainer vehicles;
    vehicles.Create(numVehicles);

    NodeContainer rsu;
    rsu.Create(1);

    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211p);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager");

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer vehicleDevices;
    vehicleDevices = wifi.Install(wifiPhy, wifiMac, vehicles);

    NetDeviceContainer rsuDevice;
    rsuDevice = wifi.Install(wifiPhy, wifiMac, rsu);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::WaypointMobilityModel",
                              "Speed", StringValue("ns3::UniformRandomVariable[Min=20.0|Max=30.0]"),
                              "PositionAllocator", StringValue("ns3::GridPositionAllocator"));
    mobility.Install(vehicles);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(rsu);

    InternetStackHelper internet;
    dsdv::DsdvHelper dsdv;
    internet.SetRoutingHelper(dsdv);
    internet.Install(vehicles);
    internet.Install(rsu);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer vehicleInterfaces;
    vehicleInterfaces = ipv4.Assign(vehicleDevices);
    Ipv4InterfaceContainer rsuInterface;
    rsuInterface = ipv4.Assign(rsuDevice);

    UdpServerHelper server(udpPort);
    ApplicationContainer serverApp = server.Install(rsu.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simDuration));

    UdpClientHelper client(rsuInterface.GetAddress(0), udpPort);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295U));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < vehicles.GetN(); ++i) {
        clientApps.Add(client.Install(vehicles.Get(i)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simDuration));

    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}