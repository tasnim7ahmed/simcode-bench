#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/nr-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging for debugging if needed
    LogComponentEnable("TcpServer", LOG_LEVEL_INFO);
    LogComponentEnable("TcpClient", LOG_LEVEL_INFO);

    // Create nodes: gNodeB + 2 UEs
    NodeContainer ueNodes;
    ueNodes.Create(2);
    NodeContainer enbNode;
    enbNode.Create(1);

    // Install NR Device and Helper
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    nrHelper->SetEpcHelper(epcHelper);

    Ptr<IdealBeamformingHelper> idealBeamformingHelper = CreateObject<IdealBeamformingHelper>();
    nrHelper->SetBeamformingHelper(idealBeamformingHelper);

    // Install internet stack on UEs and remote host
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Create a remote host connected to EPC for IP traffic
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internetRemoteHost;
    internetRemoteHost.Install(remoteHost);

    // Set up the P2P link between remoteHost and EPC
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", StringValue("100Gbps"));
    p2ph.SetChannelAttribute("Delay", StringValue("1ms"));
    NetDeviceContainer internetDevices = p2ph.Install(epcHelper->GetPgwNode(), remoteHost);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIface = ipv4h.Assign(internetDevices);
    // routing
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Configure NR deployment (1 cell, 1 sector, 1 gNB)
    NrHelper::NgcPositioning position;
    nrHelper->SetSchedulerTypeId(TypeId::LookupByName("ns3::NrMacSchedulerTdmaRR"));
    NetDeviceContainer enbDevs = nrHelper->InstallGnbDevice(enbNode);
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice(ueNodes);

    // Mobility for gNB: fixed at center
    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
    enbPositionAlloc->Add(Vector(25.0, 25.0, 10.0));
    MobilityHelper mobilityEnb;
    mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityEnb.SetPositionAllocator(enbPositionAlloc);
    mobilityEnb.Install(enbNode);

    // Mobility for UEs: random walk in 50x50 area
    MobilityHelper mobilityUe;
    mobilityUe.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                    "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                    "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    mobilityUe.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
    mobilityUe.Install(ueNodes);

    // Attach UE devices to the single cell gNB
    nrHelper->AttachToClosestEnb(ueDevs, enbDevs);

    // Assign IP address to UEs
    Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));
    // Set default gateway for UEs
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNodes.Get(i)->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Applications: TCP Server on UE 1 (index 1)
    uint16_t port = 8080;
    Address serverAddress(InetSocketAddress(ueIpIface.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = packetSinkHelper.Install(ueNodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // TCP Client on UE 0, sending to UE 1
    OnOffHelper clientHelper("ns3::TcpSocketFactory", serverAddress);
    clientHelper.SetAttribute("DataRate", StringValue("2Mbps"));
    clientHelper.SetAttribute("PacketSize", UintegerValue(512));
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer clientApp = clientHelper.Install(ueNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}