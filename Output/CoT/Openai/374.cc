#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time simTime = Seconds(10.0);

    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes: node 0 = eNB, node 1 = UE
    NodeContainer ueNodes;
    ueNodes.Create(1);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Mobility
    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
    enbPositionAlloc->Add(Vector(0.0, 0.0, 0.0));
    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.SetPositionAllocator(enbPositionAlloc);
    enbMobility.Install(enbNodes);

    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Mode", StringValue("Time"),
                                "Time", TimeValue(Seconds(1.0)),
                                "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                                "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    ueMobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                    "MinX", DoubleValue(5.0),
                                    "MinY", DoubleValue(0.0),
                                    "DeltaX", DoubleValue(10.0),
                                    "DeltaY", DoubleValue(10.0),
                                    "GridWidth", UintegerValue(1),
                                    "LayoutType", StringValue("RowFirst"));
    ueMobility.Install(ueNodes);

    // LTE helpers and devices
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    // EPC for IP stack
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Install LTE devices
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install IP stack
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP address to UE, connect to remote host via EPC
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Attach UE to eNB
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    // Add default route for UE
    Ptr<Node> ueNode = ueNodes.Get(0);
    Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting < Ipv4StaticRouting > (ueNode->GetObject<Ipv4>()->GetRoutingProtocol());
    ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);

    // Create a remote host (epcHelper provides PGW node)
    Ptr<Node> pgw = epcHelper->GetPgwNode();
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internetRemote;
    internetRemote.Install(remoteHostContainer);

    // Point-to-point link between PGW and RemoteHost
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gbps")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    NetDeviceContainer p2pDevs = p2ph.Install(pgw, remoteHost);
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(p2pDevs);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

    // Set up routing between remoteHost and UE via PGW
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // UDP application: UE sends UDP to eNB via LTE
    // Get eNB IP address (from EPC side = the remote host)
    uint16_t port = 9999;

    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(remoteHost);
    serverApps.Start(Seconds(0.01));
    serverApps.Stop(simTime);

    UdpClientHelper udpClient(remoteHostAddr, port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(10000));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(20)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(0.1));
    clientApps.Stop(simTime);

    lteHelper->EnableTraces();

    Simulator::Stop(simTime);
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}