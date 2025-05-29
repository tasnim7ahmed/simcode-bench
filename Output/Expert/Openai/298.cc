#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time simTime = Seconds(10.0);
    uint16_t numEnbNodes = 2;
    uint16_t numUeNodes = 1;
    uint16_t dlPort = 9000;

    CommandLine cmd;
    cmd.Parse(argc, argv);

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    lteHelper->SetAttribute("PathlossModel", StringValue("ns3::FriisSpectrumPropagationLossModel"));

    NodeContainer ueNodes;
    ueNodes.Create(numUeNodes);
    NodeContainer enbNodes;
    enbNodes.Create(numEnbNodes);

    // PGW node
    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Host node for remote server
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    // Connect remote host and PGW with point-to-point
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gbps")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

    // Install Internet stack on remote host
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // Assign IP to remote host side
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer remoteHostIf = ipv4h.Assign(internetDevices);

    // Routing
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Mobility for eNBs
    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                    "MinX", DoubleValue(0.0),
                                    "MinY", DoubleValue(0.0),
                                    "DeltaX", DoubleValue(100.0),
                                    "DeltaY", DoubleValue(0.0),
                                    "GridWidth", UintegerValue(2),
                                    "LayoutType", StringValue("RowFirst"));
    enbMobility.Install(enbNodes);

    // Mobility for UE: RandomWalk2dMobilityModel between eNBs
    Ptr<UniformRandomVariable> speed = CreateObject<UniformRandomVariable>();
    speed->SetAttribute("Min", DoubleValue(2.0));
    speed->SetAttribute("Max", DoubleValue(4.0));
    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(-50, 150, -50, 50)),
                                "Speed", StringValue("ns3::UniformRandomVariable[Min=2.0|Max=4.0]"),
                                "Distance", DoubleValue(10.0));
    ueMobility.SetPositionAllocator("ns3::ListPositionAllocator",
                                    "PositionList", VectorValue({Vector(0.0, 0.0, 0.0)}));
    ueMobility.Install(ueNodes);

    // Install LTE devices
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install IP stack on UEs
    InternetStackHelper internetUe;
    internetUe.Install(ueNodes);
    Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Attach UE to closest eNB initially
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    // Activate a dedicated bearer
    enum EpsBearer::Qci qci = EpsBearer::NGBR_VIDEO_TCP_DEFAULT;
    EpsBearer bearer(qci);
    lteHelper->ActivateDedicatedEpsBearer(ueLteDevs, bearer, EpcTft::Default());

    // Install Apps: UDP server on remote host
    UdpServerHelper udpServer(dlPort);
    ApplicationContainer serverApps = udpServer.Install(remoteHost);
    serverApps.Start(Seconds(0.01));
    serverApps.Stop(simTime);

    // UDP client on UE (to server IP)
    UdpClientHelper udpClient(remoteHostIf.GetAddress(0), dlPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(4294967295u));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(simTime);

    // Routing: default for UEs
    Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNodes.Get(0)->GetObject<Ipv4>());
    ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);

    // Handover scheduling: schedule after 4s, from enb0 to enb1
    Simulator::Schedule(Seconds(4.0), &LteHelper::HandoverRequest, lteHelper,
                        Seconds(4.1),
                        ueLteDevs.Get(0),
                        enbLteDevs.Get(1));

    Simulator::Stop(simTime);
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}