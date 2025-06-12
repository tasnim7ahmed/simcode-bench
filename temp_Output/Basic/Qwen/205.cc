#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/animation-interface.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VehicularNetworkSimulation");

int main(int argc, char *argv[]) {
    uint32_t numVehicles = 4;
    double simTime = 20.0;
    double packetInterval = 1.0;
    uint16_t udpPort = 9;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numVehicles", "Number of vehicles in the simulation", numVehicles);
    cmd.AddValue("simTime", "Total simulation time (seconds)", simTime);
    cmd.AddValue("packetInterval", "Interval between UDP packets (seconds)", packetInterval);
    cmd.Parse(argc, argv);

    // Create nodes for vehicles
    NodeContainer vehicles;
    vehicles.Create(numVehicles);

    // Setup WiFi in ad-hoc mode
    WifiMacHelper wifiMac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhyHelper.Default(), wifiMac, vehicles);

    // Setup mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(50.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(4),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicles);

    // Assign constant velocity to each vehicle
    for (NodeContainer::Iterator i = vehicles.Begin(); i != vehicles.End(); ++i) {
        Ptr<Node> node = *i;
        Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
        mobility->SetAttribute("Velocity", VectorValue(Vector(20.0, 0.0, 0.0))); // m/s
    }

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(vehicles);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup periodic UDP applications
    UdpServerHelper server(udpPort);
    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < numVehicles; ++i) {
        serverApps.Add(server.Install(vehicles.Get(i)));
    }
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    UdpClientHelper client;
    client.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numVehicles; ++i) {
        for (uint32_t j = 0; j < numVehicles; ++j) {
            if (i != j) {
                client.SetRemote(interfaces.GetAddress(j), udpPort);
                clientApps.Add(client.Install(vehicles.Get(i)));
            }
        }
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime));

    // Enable packet loss and delay tracing
    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpServer/Rx", MakeCallback([](Ptr<const Packet> p, const Address &addr) {
        NS_LOG_DEBUG("Received packet of size " << p->GetSize());
    }));

    // Setup animation
    AnimationInterface anim("vehicular_network.xml");
    anim.SetMobilityPollInterval(Seconds(0.1));
    for (uint32_t i = 0; i < numVehicles; ++i) {
        anim.UpdateNodeDescription(vehicles.Get(i), "Vehicle" + std::to_string(i+1));
        anim.UpdateNodeColor(vehicles.Get(i), 255, 0, 0); // Red color
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}