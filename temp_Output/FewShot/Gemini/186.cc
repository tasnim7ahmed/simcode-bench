#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/ocb-wifi-mac.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set simulation parameters
    uint32_t numVehicles = 5;
    double simulationTime = 30.0;
    double transmissionRange = 100.0; // meters
    double packetInterval = 1.0;       // seconds

    // Create nodes (vehicles)
    NodeContainer vehicles;
    vehicles.Create(numVehicles);

    // Mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                  "X", StringValue("0"),
                                  "Y", StringValue("0"),
                                  "Z", StringValue("0"),
                                  "Rho", StringValue("ns3::UniformRandomVariable[Min=0|Max=50]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-100, 100, -100, 100)),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=10]")); // 10 m/s
    mobility.Install(vehicles);

    // Configure 802.11p (WAVE)
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WaveMacHelper mac = WaveMacHelper::Default();
    Wifi80211pHelper wifi80211p;
    NetDeviceContainer devices = wifi80211p.Install(phy, mac, vehicles);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(vehicles);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Create and install application (BSM sender)
    uint16_t port = 5000;
    UdpClientHelper client(interfaces.GetAddress(0), port);
    client.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numVehicles; ++i) {
        client.SetRemote(interfaces.GetAddress(i), port);
        clientApps.Add(client.Install(vehicles.Get(i)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime));

     // Create and install sink application (BSM receiver)
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(vehicles);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    // Enable NetAnim visualization
    AnimationInterface anim("vanet_animation.xml");
    for (uint32_t i = 0; i < numVehicles; ++i) {
        anim.UpdateNodeColor(vehicles.Get(i), 0, 0, 255); // Blue for vehicles
    }

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}