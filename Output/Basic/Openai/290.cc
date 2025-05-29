#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set random seed
    RngSeedManager::SetSeed(12345);

    // Simulation parameters
    double simTime = 10.0;
    uint16_t dlPort = 5000;
    uint32_t packetSize = 1024;
    double interPacketInterval = 0.2; // 200ms

    // Create nodes
    NodeContainer ueNodes;
    ueNodes.Create(1);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Mobility for eNodeB
    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
    enbPositionAlloc->Add(Vector(50.0, 50.0, 0.0));
    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.SetPositionAllocator(enbPositionAlloc);
    enbMobility.Install(enbNodes);

    // Mobility for UE: RandomWalk2d in 100x100 area
    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0.0, 100.0, 0.0, 100.0)),
                                "Distance", DoubleValue(10.0),
                                "Speed", StringValue("ns3::ConstantRandomVariable[Constant=5.0]"));
    ueMobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                    "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                    "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    ueMobility.Install(ueNodes);

    // Install LTE Devices
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

    // Enable Turbofast tracing for PHY, MAC, RLC
    lteHelper->EnablePhyTraces();
    lteHelper->EnableMacTraces();
    lteHelper->EnableRlcTraces();

    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install IP stack on UE
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP addresses to UE
    Ipv4InterfaceContainer ueIpIfaces;
    Ptr<EpcHelper> epcHelper = lteHelper->GetEpcHelper();
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UE to eNodeB
    lteHelper->Attach(ueDevs.Get(0), enbDevs.Get(0));

    // Set default gateway
    Ipv4Address remoteAddr = ueIpIfaces.GetAddress(0);

    // Install UDP Server on UE
    UdpServerHelper udpServer(dlPort);
    ApplicationContainer serverApps = udpServer.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    // Install UDP Client on eNodeB
    UdpClientHelper udpClient(remoteAddr, dlPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(uint32_t(-1)));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(interPacketInterval)));
    udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps = udpClient.Install(enbNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simTime));

    // Start simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}