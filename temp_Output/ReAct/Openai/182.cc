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
    // Enable logging if needed
    // LogComponentEnable ("LteHelper", LOG_LEVEL_INFO);

    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes: 2 UEs, 1 eNodeB, 1 remote host
    NodeContainer ueNodes;
    ueNodes.Create(2);
    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);

    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    // Create EPC and LTE helpers
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
    lteHelper->SetEpcHelper(epcHelper);

    // Create PGW node
    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Create remote host <--> PGW link
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gbps")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));

    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);
    InternetStackHelper internet;
    internet.Install(remoteHost);

    // Assign IP to remote host
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

    // Set up routing on remote host
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Install mobility model for eNodeB and UEs
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(50.0, 50.0, 0.0)); // eNodeB position
    mobility.SetPositionAllocator(positionAlloc);

    // Position UEs
    Ptr<ListPositionAllocator> uePositionAlloc = CreateObject<ListPositionAllocator>();
    uePositionAlloc->Add(Vector(30.0, 50.0, 0.0));
    uePositionAlloc->Add(Vector(70.0, 50.0, 0.0));
    mobility.SetPositionAllocator(uePositionAlloc);
    mobility.Install(ueNodes);

    // Install LTE devices to the nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install IP stack on UEs
    InternetStackHelper ueInternet;
    ueInternet.Install(ueNodes);

    // Assign IP address to UEs
    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Attach UEs to eNodeB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        lteHelper->Attach(ueLteDevs.Get(i), enbLteDevs.Get(0));
    }

    // Optional: Activate default bearers (for TCP/IP)
    enum EpsBearer::Qci q = EpsBearer::GBR_CONV_VOICE;
    EpsBearer bearer(q);
    lteHelper->ActivateDataRadioBearer(ueLteDevs, bearer);

    // Configure default routes for UEs
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<Node> ueNode = ueNodes.Get(i);
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNode->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // TCP applications: UE0 acts as server, UE1 as client
    uint16_t port = 50000;

    // Install TCP server (PacketSink) on UE0
    Address ue0Address(InetSocketAddress(ueIpIfaces.GetAddress(0), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", ue0Address);
    ApplicationContainer sinkApps = packetSinkHelper.Install(ueNodes.Get(0));
    sinkApps.Start(Seconds(1.0));
    sinkApps.Stop(Seconds(12.0));

    // Install TCP client on UE1 targeting UE0
    OnOffHelper onOffHelper("ns3::TcpSocketFactory", ue0Address);
    onOffHelper.SetAttribute("DataRate", StringValue("1Mbps"));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));
    onOffHelper.SetAttribute("StartTime", TimeValue(Seconds(2.0)));
    onOffHelper.SetAttribute("StopTime", TimeValue(Seconds(11.0)));
    ApplicationContainer clientApps = onOffHelper.Install(ueNodes.Get(1));

    // NetAnim setup
    AnimationInterface anim("lte-ue2ue.xml");
    anim.SetConstantPosition(enbNodes.Get(0), 50.0, 50.0);
    anim.SetConstantPosition(ueNodes.Get(0), 30.0, 50.0);
    anim.SetConstantPosition(ueNodes.Get(1), 70.0, 50.0);
    anim.SetConstantPosition(remoteHost, 100.0, 50.0);

    anim.UpdateNodeColor(enbNodes.Get(0), 0, 255, 0); // eNodeB: green
    anim.UpdateNodeColor(ueNodes.Get(0), 0, 0, 255); // UE0: blue
    anim.UpdateNodeColor(ueNodes.Get(1), 255, 0, 0); // UE1: red
    anim.UpdateNodeColor(remoteHost, 128, 128, 128); // remoteHost: gray

    Simulator::Stop(Seconds(13.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}