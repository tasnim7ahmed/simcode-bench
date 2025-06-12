#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/mmwave-helper.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: 1 gNB and 2 UEs
    NodeContainer gnbNodes;
    NodeContainer ueNodes;
    gnbNodes.Create(1);
    ueNodes.Create(2);

    // Concatenate all nodes for mobility
    NodeContainer allNodes = NodeContainer(gnbNodes, ueNodes);

    // Install mobility models - gNB is stationary, UEs use RandomWalk2d
    MobilityHelper mobilityGnb;
    mobilityGnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityGnb.Install(gnbNodes);

    MobilityHelper mobilityUe;
    mobilityUe.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0.0, 100.0, 0.0, 100.0)),
                                "Distance", DoubleValue(5.0));
    mobilityUe.Install(ueNodes);

    // Install mmWave helper
    Ptr<MmWaveHelper> mmWaveHelper = CreateObject<MmWaveHelper>();
    mmWaveHelper->SetSchedulerType("ns3::MmWaveFlexTtiMacScheduler");

    NetDeviceContainer gnbDevs;
    NetDeviceContainer ueDevs;

    // Install gNB devices
    gnbDevs = mmWaveHelper->InstallGnbDevice(gnbNodes, allNodes);
    // Install UE devices
    ueDevs = mmWaveHelper->InstallUeDevice(ueNodes, allNodes);

    // Attach UEs to gNB
    mmWaveHelper->AttachToClosestGnb(ueDevs, gnbDevs);

    // Install internet stack
    InternetStackHelper internet;
    internet.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = ipv4.Assign(ueDevs);

    // Set up the UDP Echo Server on UE 1 (index 1)
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(ueNodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up the UDP Echo Client on UE 0 (index 0)
    UdpEchoClientHelper echoClient(ueIpIfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(ueNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}