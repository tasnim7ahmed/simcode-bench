#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/ngmmwave-helper.h"
#include "ns3/ngmmwave-point-to-point-epc-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint16_t numUes = 2;
    double simTime = 10.0;
    double areaSize = 50.0;
    uint16_t udpPort = 5001;
    uint32_t packetSize = 1024;
    double interPacketInterval = 0.01; // 10ms

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    Ptr<NgMmWavePointToPointEpcHelper> epcHelper = CreateObject<NgMmWavePointToPointEpcHelper>();
    Ptr<NgMmWaveHelper> mmwaveHelper = CreateObject<NgMmWaveHelper>();
    mmwaveHelper->SetEpcHelper(epcHelper);

    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(numUes);

    // Install Mobility
    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
    enbPositionAlloc->Add(Vector(areaSize / 2, areaSize / 2, 1.5));
    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.SetPositionAllocator(enbPositionAlloc);
    enbMobility.Install(enbNodes);

    MobilityHelper ueMobility;
    ueMobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                   "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                   "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                   "Z", StringValue("ns3::ConstantRandomVariable[Constant=1.5]"));

    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0.0, areaSize, 0.0, areaSize)),
                                "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                                "Distance", DoubleValue(10.0));

    ueMobility.Install(ueNodes);

    // Install NR Devices
    NetDeviceContainer enbDevs = mmwaveHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = mmwaveHelper->InstallUeDevice(ueNodes);

    // EPC: Attach all UEs to the eNB
    for (uint32_t i = 0; i < numUes; ++i)
        mmwaveHelper->AttachToClosestEnb(ueDevs.Get(i), enbDevs);

    // Internet Stack
    InternetStackHelper internet;
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Set default gateway for UEs
    for (uint32_t j = 0; j < numUes; ++j)
    {
        Ptr<Node> ueNode = ueNodes.Get(j);
        Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(ueNode->GetObject<Ipv4>()->GetRoutingProtocol());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // UDP Server on UE #1
    uint32_t serverUeIdx = 1;
    UdpServerHelper udpServer(udpPort);
    ApplicationContainer serverApps = udpServer.Install(ueNodes.Get(serverUeIdx));
    serverApps.Start(Seconds(0.1));
    serverApps.Stop(Seconds(simTime));

    // UDP Client on UE #0
    uint32_t clientUeIdx = 0;
    Ipv4Address serverAddress = ueIpIfaces.GetAddress(serverUeIdx);

    UdpClientHelper udpClient(serverAddress, udpPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000000));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(interPacketInterval)));
    udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(clientUeIdx));
    clientApps.Start(Seconds(0.2));
    clientApps.Stop(Seconds(simTime));

    // Enable PCAP tracing
    mmwaveHelper->EnableTraces();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}