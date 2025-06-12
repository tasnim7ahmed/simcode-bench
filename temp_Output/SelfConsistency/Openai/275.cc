/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Set simulation time and other parameters
    double simTime = 10.0;
    uint32_t packetSize = 512;
    uint32_t numPackets = 5;
    double interval = 1.0;

    // Create eNodeB and UE nodes
    NodeContainer enbNodes;
    enbNodes.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Install Mobility
    MobilityHelper mobility;

    // eNB: fixed position in the center
    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator> ();
    enbPositionAlloc->Add (Vector (50.0, 50.0, 0.0));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.SetPositionAllocator (enbPositionAlloc);
    mobility.Install (enbNodes);

    // UE: random walk in 100x100 area
    mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue ("Uniform[0.0|100.0]"),
                                  "Y", StringValue ("Uniform[0.0|100.0]"));
    mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                               "Bounds", RectangleValue (Rectangle (0, 100, 0, 100)),
                               "Distance", DoubleValue (20.0)); // some move per step
    mobility.Install (ueNodes);

    // LTE Devices
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on UEs and remote host
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Get the PGW node
    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Create remote host behind PGW, acting as the simple server
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internetHost;
    internetHost.Install (remoteHost);

    // Setup point-to-point link between PGW and remoteHost
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
    p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));
    NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);

    // Assign IP addresses
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);

    // Interface 1 is the one on remoteHost
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);

    // For UE, assign IP address via EPC
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));

    // Attach UE to eNB
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    // Install UDP server on remoteHost (acts as eNB-side application endpoint)
    uint16_t udpPort = 4000;
    UdpServerHelper udpServer (udpPort);
    ApplicationContainer serverApps = udpServer.Install (remoteHost);
    serverApps.Start (Seconds (0.0));
    serverApps.Stop (Seconds (simTime));

    // Install UDP client on UE
    UdpClientHelper udpClient (remoteHostAddr, udpPort);
    udpClient.SetAttribute ("MaxPackets", UintegerValue (numPackets));
    udpClient.SetAttribute ("Interval", TimeValue (Seconds (interval)));
    udpClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
    ApplicationContainer clientApps = udpClient.Install (ueNodes.Get(0));
    clientApps.Start (Seconds (1.0));
    clientApps.Stop (Seconds (simTime));

    // Enable routing
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
    remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}