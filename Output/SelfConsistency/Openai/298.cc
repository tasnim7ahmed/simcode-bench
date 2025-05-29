#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LteHandoverUdpExample");

int
main (int argc, char *argv[])
{
    // Enable logging for debugging (optional)
    // LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    // LogComponentEnable ("LteHandoverUdpExample", LOG_LEVEL_INFO);

    double simTime = 10.0;

    // Create nodes: 2 eNodeB, 1 UE, 1 remote host
    NodeContainer ueNodes;
    NodeContainer enbNodes;
    ueNodes.Create (1);
    enbNodes.Create (2);
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create (1);
    Ptr<Node> remoteHost = remoteHostContainer.Get (0);

    // Set up internet
    InternetStackHelper internet;
    internet.Install (remoteHost);

    // Create EPC helper, LTE helper
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
    lteHelper->SetEpcHelper (epcHelper);

    // Assign IP addresses to devices on EPC
    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Set up remoteHost <-> PGW via PointToPoint link
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
    p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));
    NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);

    // Assign IP address to remoteHost
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);

    // Install internet stack on UE (after EPC is created)
    internet.Install (ueNodes);

    // Mobility for eNodeBs
    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    enbMobility.Install (enbNodes);

    // Position eNodeBs at (0,0,0) and (100,0,0)
    Ptr<MobilityModel> enb0Mob = enbNodes.Get(0)->GetObject<MobilityModel> ();
    Ptr<MobilityModel> enb1Mob = enbNodes.Get(1)->GetObject<MobilityModel> ();
    enb0Mob->SetPosition (Vector (0.0, 0.0, 0.0));
    enb1Mob->SetPosition (Vector (100.0, 0.0, 0.0));

    // UE Mobility with RandomWalk2d between the eNodeBs
    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                 "Bounds", RectangleValue (Rectangle (0, 100, -10, 10)),
                                 "Distance", DoubleValue (100),
                                 "Mode", StringValue ("Time"),
                                 "Time", TimeValue (Seconds (1.0)),
                                 "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=15.0]"));
    ueMobility.Install (ueNodes);
    ueNodes.Get(0)->GetObject<MobilityModel> ()->SetPosition (Vector (0.0, 0.0, 0.0));

    // Install LTE Devices to the nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

    // Attach UE to the first eNodeB
    lteHelper->Attach (ueLteDevs.Get(0), enbLteDevs.Get(0));

    // Activate default EPS bearer for the UE
    enum EpsBearer::Qci qci = EpsBearer::GBR_CONV_VOICE;
    EpsBearer bearer (qci);
    lteHelper->ActivateDedicatedEpsBearer (ueLteDevs, bearer, EpcTft::Default ());

    // Assign IP Address to UEs
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));

    // Set up routing: remote host static route to UE subnet via PGW
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
    remoteHostStaticRouting->AddNetworkRouteTo ("7.0.0.0", "255.0.0.0", 1);

    // Set the default route for the UE via gateway
    Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNodes.Get(0)->GetObject<Ipv4> ());
    ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);

    // Application: UDP server on RemoteHost, UDP client on UE
    uint16_t udpPort = 9000;
    UdpServerHelper udpServer (udpPort);
    ApplicationContainer serverApps = udpServer.Install (remoteHost);
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (simTime));

    UdpClientHelper udpClient (remoteHostAddr, udpPort);
    udpClient.SetAttribute ("MaxPackets", UintegerValue (32000));
    udpClient.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
    udpClient.SetAttribute ("PacketSize", UintegerValue (1024));
    ApplicationContainer clientApps = udpClient.Install (ueNodes.Get (0));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (simTime));

    // Schedule handover during simulation from enb0 to enb1
    Simulator::Schedule (Seconds (5.0), &LteHelper::HandoverRequest,
                         lteHelper,
                         Seconds (5.0),
                         ueLteDevs.Get (0),
                         enbLteDevs.Get (1));

    // Enable tracing
    lteHelper->EnableTraces ();

    // Run simulation
    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}