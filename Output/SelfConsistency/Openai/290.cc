/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteSingleEnbUeUdpExample");

int
main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes: one eNodeB and one UE
    NodeContainer enbNodes;
    NodeContainer ueNodes;
    enbNodes.Create(1);
    ueNodes.Create(1);

    // Install mobility for eNodeB (fixed)
    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
    enbPositionAlloc->Add(Vector(50.0, 50.0, 0.0));
    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.SetPositionAllocator(enbPositionAlloc);
    enbMobility.Install(enbNodes);

    // Mobility for UE: random waypoint within 100x100
    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel(
        "ns3::RandomWaypointMobilityModel",
        "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
        "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"),
        "PositionAllocator", PointerValue(
            CreateObjectWithAttributes<RandomRectanglePositionAllocator>(
                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]")
            )
        )
    );
    ueMobility.Install(ueNodes);

    // Install LTE devices
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetAttribute("UseCa", BooleanValue(false));

    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on the UEs
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP addresses to UEs, using the EPC helper
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // We need a remote host to serve as anchor point to outside the LTE system
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internetRemote;
    internetRemote.Install(remoteHost);

    // Create the Internet backbone between EPC and the remote host
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));

    NetDeviceContainer internetDevices = p2ph.Install(epcHelper->GetPgwNode(), remoteHost);
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);

    // Interface 0 is loopback, 1 is p2p
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

    // Assign IP to UE, and route packets from it via EPC's gateway
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UE to eNodeB
    lteHelper->Attach(ueDevs.Get(0), enbDevs.Get(0));

    // Allow remote host to reach UE
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<Node> ueNode = ueNodes.Get(i);
        Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(ueNode->GetObject<Ipv4>()->GetRoutingProtocol());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Applications
    uint16_t udpPort = 5000;

    // Install UDP server on the UE
    UdpServerHelper udpServer(udpPort);
    ApplicationContainer serverApps = udpServer.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(0.1));
    serverApps.Stop(Seconds(10.0));

    // Install UDP client on RemoteHost
    UdpClientHelper udpClient(ueIpIfaces.GetAddress(0), udpPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(4294967295u));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(200)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = udpClient.Install(remoteHost);
    clientApps.Start(Seconds(0.5));
    clientApps.Stop(Seconds(10.0));

    // Enable LTE tracing
    lteHelper->EnablePhyTraces();
    lteHelper->EnableMacTraces();
    lteHelper->EnableRlcTraces();

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}