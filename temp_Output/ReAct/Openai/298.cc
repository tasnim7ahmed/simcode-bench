#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/epc-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Simulation parameters
    double simTime = 10.0;

    // Nodes: 2 eNodeBs, 1 UE
    NodeContainer enbNodes;
    enbNodes.Create(2);

    NodeContainer ueNodes;
    ueNodes.Create(1);

    // PGW node via EPC helper:
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
    lteHelper->SetEpcHelper(epcHelper);

    // Install mobility for eNodeBs (static)
    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.Install(enbNodes);

    Ptr<MobilityModel> mmEnb0 = enbNodes.Get(0)->GetObject<MobilityModel>();
    mmEnb0->SetPosition(Vector(0.0, 0.0, 0.0));

    Ptr<MobilityModel> mmEnb1 = enbNodes.Get(1)->GetObject<MobilityModel>();
    mmEnb1->SetPosition(Vector(100.0, 0.0, 0.0));

    // Install mobility for UE (RandomWalk2d to move between eNodeBs)
    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                 "Bounds", RectangleValue (Rectangle (0, 100, -20, 20)),
                                 "Distance", DoubleValue (100.0),
                                 "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=10]"));
    ueMobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue (0.0),
                                     "MinY", DoubleValue (0.0),
                                     "DeltaX", DoubleValue (100.0),
                                     "DeltaY", DoubleValue (100.0),
                                     "GridWidth", UintegerValue (1),
                                     "LayoutType", StringValue ("RowFirst"));
    ueMobility.Install(ueNodes);

    // Install LTE Devices
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Attach UE to eNodeB 0 initially
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    // Install IP stack on UE and remote host
    InternetStackHelper internet;
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    internet.Install(ueNodes);
    internet.Install(remoteHostContainer);

    // Assign IP address to UE
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Set the default gateway for the UE
    Ptr<Node> ue = ueNodes.Get(0);
    Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting> (ue->GetObject<Ipv4> ()->GetRoutingProtocol ());
    ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);

    // Create remote host connected to PGW
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Set up internet connection from PGW to RemoteHost
    NodeContainer pgwRemoteHost;
    pgwRemoteHost.Add(pgw);
    pgwRemoteHost.Add(remoteHost);

    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate ("100Gbps")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    NetDeviceContainer internetDevices = p2ph.Install(pgwRemoteHost);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);

    // Assign IP address to remoteHost
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(remoteHost->GetObject<Ipv4>()->GetRoutingProtocol());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Install application: UDP server on eNodeB 0
    uint16_t port = 9999;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(enbNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    // Install UDP client on UE, directing traffic to eNodeB 0's IP
    Ipv4Address enbServerAddr = enbNodes.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
    UdpClientHelper udpClient(enbServerAddr, port);
    udpClient.SetAttribute ("MaxPackets", UintegerValue (1000000));
    udpClient.SetAttribute ("Interval", TimeValue (MilliSeconds (10)));
    udpClient.SetAttribute ("PacketSize", UintegerValue (512));
    ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime));

    // Handover: schedule midway through simulation
    Simulator::Schedule(Seconds(simTime/2.0), &LteHelper::HandoverRequest, lteHelper, Seconds(simTime/2.0), ueLteDevs.Get(0), enbLteDevs.Get(1));

    // Enable traces (optional)
    lteHelper->EnableTraces();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}