#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/dsdv-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetDsdvSimulation");

int main(int argc, char *argv[]) {
    uint32_t numVehicles = 10;
    double simDuration = 50.0;
    double speed = 20.0; // m/s
    double interVehicleDistance = 50.0; // meters

    CommandLine cmd;
    cmd.AddValue("numVehicles", "Number of vehicles in the simulation", numVehicles);
    cmd.AddValue("simDuration", "Duration of simulation in seconds", simDuration);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer vehicles;
    vehicles.Create(numVehicles);

    // Setup mobility
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < numVehicles; ++i) {
        positionAlloc->Add(Vector(i * interVehicleDistance, 0.0, 0.0));
    }

    MobilityHelper mobility;
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicles);

    for (NodeContainer::Iterator it = vehicles.Begin(); it != vehicles.End(); ++it) {
        Ptr<ConstantVelocityMobilityModel> velModel =
            (*it)->GetObject<ConstantVelocityMobilityModel>();
        velModel->SetVelocity(Vector(speed, 0.0, 0.0));
    }

    // Setup WiFi
    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    wifiPhy.SetPreambleDetectionModel("ns3::PreambleDetectionModelDefault");
    wifiPhy.SetErrorRateModel("ns3::YansErrorRateModel");

    wifiMac.SetType("ns3::AdhocWifiMac");
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, vehicles);

    // Install Internet stack with DSDV routing
    DsdvHelper dsdv;
    dsdv.Set("PeriodicUpdateInterval", TimeValue(Seconds(15)));
    dsdv.Set("SettlingTime", TimeValue(Seconds(5)));

    Ipv4ListRoutingHelper list;
    list.Add(dsdv, 100);

    InternetStackHelper internet;
    internet.SetRoutingHelper(list);
    internet.Install(vehicles);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup applications
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(vehicles.Get(numVehicles - 1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simDuration));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(numVehicles - 1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue((uint32_t)simDuration * 10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numVehicles - 1; ++i) {
        clientApps.Add(echoClient.Install(vehicles.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simDuration - 1));

    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Enable pcap tracing
    wifiPhy.EnablePcap("vanet_dsdv_simulation", devices);

    // Setup FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        NS_LOG_INFO("Flow ID: " << iter->first << " Src Addr " << iter->second.txPackets);
    }

    Simulator::Destroy();
    return 0;
}