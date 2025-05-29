#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LteUdpSimpleExample");

int
main (int argc, char *argv[])
{
    // Set simulation time and packet params
    uint32_t nPackets = 1000;
    Time interPacketInterval = MilliSeconds (20);
    uint16_t udpPort = 12345;

    // Enable logging
    LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

    // Create nodes: 1 eNB and 1 UE
    NodeContainer ueNodes;
    ueNodes.Create (1);
    NodeContainer enbNodes;
    enbNodes.Create (1);

    // Setup LTE Helpers
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
    lteHelper->SetEpcHelper (epcHelper);

    // Create remote host (for packet gateway)
    Ptr<Node> remoteHost = CreateObject<Node> ();
    NodeContainer remoteHostContainer;
    remoteHostContainer.Add (remoteHost);
    InternetStackHelper internet;
    internet.Install (remoteHostContainer);

    // Create point to point link between PGW and remoteHost
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
    p2ph.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (1)));
    NetDeviceContainer internetDevices = p2ph.Install (epcHelper->GetPgwNode (), remoteHost);

    // Assign IP addresses to remote side
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);

    // Install mobility model
    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (enbNodes);
    mobility.Install (ueNodes);

    // LTE devices on nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

    // Install IP stack on UE
    internet.Install (ueNodes);
    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));

    // Attach UE to eNB
    lteHelper->Attach (ueLteDevs.Get (0), enbLteDevs.Get (0));

    // Set Default Gateway for UE
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNodes.Get (0)->GetObject<Ipv4> ());
    ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);

    // Install UDP server app on UE
    UdpServerHelper udpServer (udpPort);
    ApplicationContainer serverApps = udpServer.Install (ueNodes.Get (0));
    serverApps.Start (Seconds (0.1));
    serverApps.Stop (Seconds (nPackets * interPacketInterval.GetSeconds () + 2.0));

    // Install UDP client app on remote host, sending to UE
    UdpClientHelper udpClient (ueIpIfaces.GetAddress (0), udpPort);
    udpClient.SetAttribute ("MaxPackets", UintegerValue (nPackets));
    udpClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
    udpClient.SetAttribute ("PacketSize", UintegerValue (1024));
    ApplicationContainer clientApps = udpClient.Install (remoteHost);
    clientApps.Start (Seconds (1.0));
    clientApps.Stop (Seconds (nPackets * interPacketInterval.GetSeconds () + 1.5));

    // Enable routing on remoteHost
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
    remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

    // Run simulation
    Simulator::Stop (Seconds (nPackets * interPacketInterval.GetSeconds () + 3.0));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}