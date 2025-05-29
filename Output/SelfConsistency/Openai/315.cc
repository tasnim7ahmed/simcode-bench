/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"

// NR & 5G Core
#include "ns3/antenna-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/nr-module.h"
#include "ns3/nr-point-to-point-epc-helper.h" // for 5G Core/EPC

using namespace ns3;

int 
main (int argc, char *argv[])
{
    // Command line parsing, if needed
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes: 1 UE, 1 gNB
    NodeContainer ueNodes;
    ueNodes.Create (1);
    NodeContainer gnbNodes;
    gnbNodes.Create (1);

    // Install mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (ueNodes);
    mobility.Install (gnbNodes);
    ueNodes.Get (0)->AggregateObject(CreateObjectWithAttributes<MobilityModel> ("Position", VectorValue(Vector(10,0,0))));
    gnbNodes.Get (0)->AggregateObject(CreateObjectWithAttributes<MobilityModel> ("Position", VectorValue(Vector(0,0,0))));

    // Install NR/EPC helpers
    Ptr<NrPointToPointEpcHelper> epcHelper = CreateObject<NrPointToPointEpcHelper> ();
    Ptr<Node> pgw = epcHelper->GetPgwNode ();

    // NR Helper
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper> ();
    nrHelper->SetEpcHelper (epcHelper);

    // Use simplified channel and loss models for speed
    nrHelper->SetAttribute ("ChannelConditionModel", StringValue ("ns3::IdealBeamformingHelper"));
    nrHelper->SetPathlossAttribute ("ShadowingEnabled", BooleanValue (false));
    nrHelper->SetSchedulerType ("ns3::NrMacSchedulerTdmaRR");

    // Configure gNB and UE NetDevices
    NetDeviceContainer gnbDevs = nrHelper->InstallGnbDevice (gnbNodes, {28e9});
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice (ueNodes, {28e9});
    // Attach UE to gNB
    nrHelper->AttachToClosestEnb (ueDevs, gnbDevs);

    // IP stack
    InternetStackHelper internet;
    internet.Install (ueNodes);

    // Assign IP address to UE
    Ipv4InterfaceContainer ueIfaces = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueDevs));

    // Set default route for UE via EPC
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNodes.Get(0)->GetObject<Ipv4>());
    ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);

    // Connect remote host to PGW (core network) via point-to-point to simulate external traffic
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    InternetStackHelper internetHost;
    internetHost.Install(remoteHost);

    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue (DataRate ("100Gbps")));
    p2ph.SetChannelAttribute("Delay", TimeValue (Seconds(0.0001)));

    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIfaces = ipv4h.Assign(internetDevices);
    // Access IP for remoteHost and PGW
    Ipv4Address remoteHostAddr = internetIfaces.GetAddress(1);

    // Configure static routing on remote host to reach UE
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(
        Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Install UDP server on UE (port 9)
    uint16_t serverPort = 9;
    UdpServerHelper udpServer(serverPort);
    ApplicationContainer serverApps = udpServer.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Install UDP client on remote host to send 1000 packets of 1024 bytes, every 0.01 seconds
    uint32_t maxPackets = 1000;
    Time interPacketInterval = Seconds(0.01);
    uint32_t packetSize = 1024;
    UdpClientHelper udpClient(ueIfaces.GetAddress(0), serverPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    udpClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps = udpClient.Install(remoteHost);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable NR logs for debug (optional)
    //LogComponentEnable("NrHelper", LOG_LEVEL_INFO);

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}