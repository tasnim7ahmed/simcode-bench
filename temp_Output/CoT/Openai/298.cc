#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/point-to-point-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LteHandoverUdpExample");

int main (int argc, char *argv[])
{
    Time simTime = Seconds (10.0);

    // Create nodes: 2 eNBs, 1 UE, 1 remote host (to act as UDP server)
    NodeContainer ueNodes;
    ueNodes.Create (1);

    NodeContainer enbNodes;
    enbNodes.Create (2);

    NodeContainer remoteHostContainer;
    remoteHostContainer.Create (1);
    Ptr<Node> remoteHost = remoteHostContainer.Get (0);

    // Install internet stack on remote host and UE
    InternetStackHelper internet;
    internet.Install (remoteHostContainer);
    internet.Install (ueNodes);

    // Set up P2P backhaul between PGW and remote host
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
    p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));

    // Create LTE Helpers and EPC
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
    lteHelper->SetEpcHelper (epcHelper);

    Ptr<Node> pgw = epcHelper->GetPgwNode ();

    // Connect remote host to PGW
    NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);

    // Set up static routing on remote host --> UE packet forwarding
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
    remoteHostStaticRouting->AddNetworkRouteTo ("7.0.0.0", "255.0.0.0", 1);

    // Mobility: eNBs at fixed positions, UE moves between them
    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    enbMobility.Install (enbNodes);

    Ptr<MobilityModel> m0 = enbNodes.Get (0)->GetObject<MobilityModel> ();
    m0->SetPosition (Vector (0.0, 0.0, 0.0));
    Ptr<MobilityModel> m1 = enbNodes.Get (1)->GetObject<MobilityModel> ();
    m1->SetPosition (Vector (100.0, 0.0, 0.0));

    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue (Rectangle (0.0, 100.0, -10.0, 10.0)),
                                "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=10.0]"));
    ueMobility.Install (ueNodes);
    ueNodes.Get (0)->GetObject<MobilityModel> ()->SetPosition (Vector (0.0, 0.0, 0.0));

    // Install devices: eNBs and UEs
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

    // Attach UE to eNB 0 at start
    lteHelper->Attach (ueLteDevs.Get (0), enbLteDevs.Get (0));

    // Activate default EPS bearer
    enum EpsBearer::Qci qci = EpsBearer::GBR_CONV_VOICE;
    EpsBearer bearer (qci);
    lteHelper->ActivateDataRadioBearer (ueLteDevs, bearer);

    // Assign IP addresses to UEs
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));

    // Set up routing for UE
    Ptr<Node> ueNode = ueNodes.Get (0);
    Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
    ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);

    // Install UDP server on remote host
    uint16_t udpPort = 4000;
    UdpServerHelper udpServer (udpPort);
    ApplicationContainer serverApp = udpServer.Install (remoteHost);
    serverApp.Start (Seconds (0.0));
    serverApp.Stop (simTime);

    // Install UDP client on UE (sending data to remote host)
    uint32_t packetSize = 1024;
    uint32_t maxPacketCount = 3200;
    Time interPacketInterval = Seconds (0.003); // ~3ms (about 333 packets/sec)
    UdpClientHelper udpClient (remoteHostAddr, udpPort);
    udpClient.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
    udpClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
    udpClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
    ApplicationContainer clientApp = udpClient.Install (ueNodes.Get (0));
    clientApp.Start (Seconds (1.0));
    clientApp.Stop (simTime);

    // Enable handover: allow automatic handover trigger
    lteHelper->AddX2Interface (enbNodes);

    // Enable LTE traces (optional)
    lteHelper->EnableTraces ();

    Simulator::Stop (simTime);
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}