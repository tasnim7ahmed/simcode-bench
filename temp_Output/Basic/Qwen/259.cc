#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mmwave-helper.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FiveGmmWaveSimulation");

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: 1 gNB and 2 UEs
    NodeContainer gnbNode;
    gnbNode.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(2);

    // Mobility models for UEs (Random Walk)
    MobilityHelper mobilityUe;
    mobilityUe.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)),
                                "Distance", DoubleValue(1.0));
    mobilityUe.Install(ueNodes);

    // Mobility model for gNB (stationary)
    MobilityHelper mobilityGnb;
    mobilityGnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityGnb.Install(gnbNode);

    // Position the gNB at origin
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // gNB position
    positionAlloc->Add(Vector(10.0, 0.0, 0.0)); // UE1 position
    positionAlloc->Add(Vector(20.0, 0.0, 0.0)); // UE2 position

    MobilityHelper mobility;
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(gnbNode);
    mobility.Install(ueNodes);

    // Install mmWave devices
    mmwave::MmWaveHelper mmwaveHelper;
    mmwaveHelper.Initialize();

    NetDeviceContainer gnbDevs;
    gnbDevs = mmwaveHelper.InstallGnbDevice(gnbNode, ueNodes);

    NetDeviceContainer ueDevs;
    ueDevs = mmwaveHelper.InstallUeDevice(ueNodes, gnbNode);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(gnbNode);
    internet.Install(ueNodes);

    // Assign IPv4 addresses in 192.168.1.0/24
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer ueInterfaces;
    ueInterfaces = ipv4.Assign(ueDevs);

    // Setup applications
    uint16_t port = 9; // UDP echo server port

    // Server on UE 2
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(ueNodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Client on UE 0, connecting to UE 2's IP
    UdpEchoClientHelper echoClient(ueInterfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Simulation setup
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}