#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Create nodes: 2 UEs, 1 eNodeB, 1 remote host for the EPC core
    NodeContainer ueNodes;
    ueNodes.Create(2);
    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    // Install the internet stack on remote host (EPC gateway will get stack later)
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // Create LTE Helper & EPC Helper
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Create the EPC P-GW <-> remote host link
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
    p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));
    NetDeviceContainer internetDevices = p2ph.Install(epcHelper->GetPgwNode(), remoteHost);

    // Assign IP addresses to remote host link
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    // Assign IP to remote host (the second device)
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);

    // Routing for remote host: route to UEs through EPC gateway
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
    remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

    // Configure mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    // Install LTE Devices to the nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on UEs
    internet.Install(ueNodes);

    // Assign IP addresses to UEs, and attach to the eNodeB
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        lteHelper->Attach(ueLteDevs.Get(i), enbLteDevs.Get(0));
    }

    // Add default route for UEs to the EPC gateway
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNodes.Get(i)->GetObject<Ipv4> ());
        ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

    // Setup TCP server app on UE 0
    uint16_t serverPort = 8080;
    Address serverAddress (InetSocketAddress (ueIpIfaces.GetAddress (0), serverPort));
    PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), serverPort));
    ApplicationContainer serverApp = packetSinkHelper.Install (ueNodes.Get(0));
    serverApp.Start (Seconds (1.0));
    serverApp.Stop (Seconds (10.0));

    // Setup TCP client app on UE 1, sending 1,000,000 bytes
    OnOffHelper clientHelper ("ns3::TcpSocketFactory", serverAddress);
    clientHelper.SetAttribute ("DataRate", DataRateValue (DataRate ("10Mb/s")));
    clientHelper.SetAttribute ("PacketSize", UintegerValue (1000));
    clientHelper.SetAttribute ("MaxBytes", UintegerValue (1000000));
    clientHelper.SetAttribute ("StartTime", TimeValue (Seconds (2.0)));
    clientHelper.SetAttribute ("StopTime", TimeValue (Seconds (10.0)));
    ApplicationContainer clientApp = clientHelper.Install(ueNodes.Get(1));

    // Enable LTE and TCP logging if needed
    // LogComponentEnable("LteHelper", LOG_LEVEL_INFO);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}