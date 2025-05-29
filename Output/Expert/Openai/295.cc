#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-epc-helper.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Create nodes: 1 eNodeB, 1 UE
    NodeContainer enbNodes;
    enbNodes.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Install Mobility
    // eNodeB: stationary
    MobilityHelper mobilityEnb;
    mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityEnb.Install(enbNodes);

    // UE: random mobility
    MobilityHelper mobilityUe;
    mobilityUe.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                    "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                    "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    mobilityUe.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0.0, 50.0, 0.0, 50.0)),
                                "Distance", DoubleValue(10.0));
    mobilityUe.Install(ueNodes);

    // Install LTE Devices
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install IP stack
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP address to UEs, and set default gateway
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Attach UE to eNodeB
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    // Set up default bearer
    enum EpsBearer::Qci qci = EpsBearer::NGBR_VIDEO_TCP_DEFAULT;
    EpsBearer bearer(qci);
    lteHelper->ActivateDataRadioBearer(ueLteDevs, bearer);

    // Create UDP server on UE
    uint16_t port = 4000;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Create UDP client on eNodeB side (simulate as a remote host behind PGW)
    // Create remote host node (behind the EPC PGW)
    Ptr<Node> remoteHost;
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    remoteHost = remoteHostContainer.Get(0);

    // Install internet stack on remote host
    InternetStackHelper internetHost;
    internetHost.Install(remoteHost);

    // Get SGW/PGW node
    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Create point-to-point link between remoteHost and PGW
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gbps")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));

    NetDeviceContainer p2pDevs = p2ph.Install(pgw, remoteHost);

    // Assign IP address to remote host
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer remoteHostIf = ipv4h.Assign(p2pDevs);

    // Add static route on remote host to UE subnet via link to PGW
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Create UDP client on remote host towards UE
    UdpClientHelper client(ueIpIface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(10000));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(remoteHost);
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    lteHelper->EnableTraces();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}