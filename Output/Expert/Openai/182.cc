#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time simTime = Seconds(10.0);

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    Ptr<Node> pgw = epcHelper->GetPgwNode();

    NodeContainer ueNodes;
    ueNodes.Create(2);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Install Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));      // eNodeB
    positionAlloc->Add(Vector(100.0, 0.0, 0.0));    // UE1
    positionAlloc->Add(Vector(200.0, 0.0, 0.0));    // UE2

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    // Install LTE Devices to the nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on UEs
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP address to UEs, and attach to eNodeB
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));
    lteHelper->Attach(ueLteDevs.Get(1), enbLteDevs.Get(0));

    lteHelper->ActivateDataRadioBearer(ueLteDevs, EpsBearer(EpsBearer::NGBR_VIDEO_TCP_DEFAULT), EpcTft::Default());

    // Configure default routes for UEs
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<Node> ueNode = ueNodes.Get(i);
        Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting>(ueNode->GetObject<Ipv4>()->GetRoutingProtocol());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Install IP stack on remote host (simulate as a node behind PGW)
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    InternetStackHelper internetRemote;
    internetRemote.Install(remoteHostContainer);

    // Create the Internet (remote host) <-> PGW connection
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gbps")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));

    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHostContainer.Get(0));

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIfaces = ipv4h.Assign(internetDevices);

    Ipv4Address remoteHostAddr = internetIfaces.GetAddress(1);

    // Add route to remote host
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(
        remoteHostContainer.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // TCP traffic: UE 0 acts as client, UE 1 as server.
    uint16_t servPort = 5000;
    Address serverAddress(InetSocketAddress(ueIpIfaces.GetAddress(1), servPort));

    // Install packet sink (server) on UE 1
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), servPort));
    ApplicationContainer sinkApp = sinkHelper.Install(ueNodes.Get(1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(simTime);

    // Install TCP client on UE 0
    OnOffHelper clientHelper("ns3::TcpSocketFactory", serverAddress);
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("2Mbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper.SetAttribute("StartTime", TimeValue(Seconds(2.0)));
    clientHelper.SetAttribute("StopTime", TimeValue(simTime - Seconds(1.0)));
    ApplicationContainer clientApp = clientHelper.Install(ueNodes.Get(0));

    // NetAnim setup
    AnimationInterface anim("lte-ue-enb.netanim.xml");

    anim.SetConstantPosition(enbNodes.Get(0), 0.0, 0.0, 0.0);
    anim.SetConstantPosition(ueNodes.Get(0), 100.0, 0.0, 0.0);
    anim.SetConstantPosition(ueNodes.Get(1), 200.0, 0.0, 0.0);
    anim.SetConstantPosition(remoteHostContainer.Get(0), -100.0, 0.0, 0.0);

    Simulator::Stop(simTime);
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}