#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/wave-helper.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VANETExample");

int main(int argc, char *argv[])
{
    // Set simulation time in seconds
    double simulationTime = 10.0;
    
    // Create 2 nodes representing vehicles
    NodeContainer vehicles;
    vehicles.Create(2);

    // Set up WAVE (802.11p) PHY and MAC for VANET
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    NqosWaveMacHelper wifiMac = NqosWaveMacHelper::Default();
    Wifi80211pHelper waveHelper = Wifi80211pHelper::Default();
    NetDeviceContainer devices = waveHelper.Install(wifiPhy, wifiMac, vehicles);

    // Set mobility model for the vehicles
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicles);

    // Set initial positions and constant speeds for vehicles
    Ptr<ConstantVelocityMobilityModel> mobility1 = vehicles.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    mobility1->SetPosition(Vector(0.0, 0.0, 0.0));
    mobility1->SetVelocity(Vector(20.0, 0.0, 0.0)); // Vehicle 1 moves at 20 m/s

    Ptr<ConstantVelocityMobilityModel> mobility2 = vehicles.Get(1)->GetObject<ConstantVelocityMobilityModel>();
    mobility2->SetPosition(Vector(100.0, 0.0, 0.0));
    mobility2->SetVelocity(Vector(20.0, 0.0, 0.0)); // Vehicle 2 moves at 20 m/s

    // Install Internet stack on vehicles
    InternetStackHelper internet;
    internet.Install(vehicles);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install UDP server on vehicle 1
    uint16_t port = 4000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(vehicles.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simulationTime));

    // Install UDP client on vehicle 2, sending packets to vehicle 1
    UdpClientHelper udpClient(interfaces.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(10));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = udpClient.Install(vehicles.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(simulationTime));

    // Set up NetAnim
    AnimationInterface anim("vanet_netanim.xml");

    // Set positions for visualization in NetAnim
    anim.SetConstantPosition(vehicles.Get(0), 0.0, 0.0);  // Vehicle 1
    anim.SetConstantPosition(vehicles.Get(1), 100.0, 0.0); // Vehicle 2

    // Enable packet metadata for NetAnim
    anim.EnablePacketMetadata(true);

    // Run the simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

