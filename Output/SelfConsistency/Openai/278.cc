#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteHttpRandomWalkExample");

int main(int argc, char *argv[])
{
    // Enable logging for debugging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes: one eNodeB and one UE
    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Position eNodeB statically
    MobilityHelper enbMobility;
    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
    enbPositionAlloc->Add(Vector(25.0, 25.0, 0.0));
    enbMobility.SetPositionAllocator(enbPositionAlloc);
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.Install(enbNodes);

    // UE mobility: Random Walk in 50x50
    MobilityHelper ueMobility;
    ueMobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                   "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                   "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0.0, 50.0, 0.0, 50.0)),
                                "Distance", DoubleValue(10.0),
                                "Time", TimeValue(Seconds(1.0)));
    ueMobility.Install(ueNodes);

    // Create remote host (Internet Gateway)
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // Create PGW (Packet Gateway)
    Ptr<Node> pgw;

    // LTE helpers setup
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    pgw = epcHelper->GetPgwNode();

    // Create remote host - PGW link (core network)
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.01)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

    // Assign IP address to the remote side
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);

    // Routing for remoteHost
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Install LTE devices
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install IP stack on UEs
    internet.Install(ueNodes);

    // Assign IP addresses to UEs
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Attach UE to the eNodeB
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    // Set the default gateway for the UE
    Ptr<Node> ue = ueNodes.Get(0);
    Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ue->GetObject<Ipv4>());
    ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);

    // Install UDP server on eNodeB
    // The LTE eNodeB is not a general-purpose host; typically the application server is in the core network.
    // So, install UDP server on remoteHost (Internet side) to simulate HTTP server.
    uint16_t port = 80;
    UdpServerHelper serverHelper(port);
    ApplicationContainer serverApps = serverHelper.Install(remoteHost);
    serverApps.Start(Seconds(0.1));
    serverApps.Stop(Seconds(10.0));

    // Install UDP client on UE to simulate HTTP client
    UdpClientHelper clientHelper(internetIpIfaces.GetAddress(1), port);
    clientHelper.SetAttribute("MaxPackets", UintegerValue(5));
    clientHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    clientHelper.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = clientHelper.Install(ue);
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // Enable checksum
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    Simulator::Stop(Seconds(10.1));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}