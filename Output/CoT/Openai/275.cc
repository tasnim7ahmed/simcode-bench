#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer ueNode;
    ueNode.Create(1);
    NodeContainer enbNode;
    enbNode.Create(1);

    // Install mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(50.0, 50.0, 0.0)); // eNB at center
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNode);

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(0.0, 100.0, 0.0, 100.0)));
    mobility.Install(ueNode);

    // Install LTE devices to the nodes
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

    NetDeviceContainer enbDevs;
    NetDeviceContainer ueDevs;
    enbDevs = lteHelper->InstallEnbDevice(enbNode);
    ueDevs = lteHelper->InstallUeDevice(ueNode);

    // Install the IP stack on the UEs
    InternetStackHelper internet;
    internet.Install(ueNode);

    // Assign IP addresses to UEs, set up EPC helper for core network
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Create a remote host
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internetHost;
    internetHost.Install(remoteHostContainer);

    // Create the PGW and connect remoteHost via point-to-point
    Ptr<Node> pgw = epcHelper->GetPgwNode();
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gbps")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

    // Route remoteHost ↔️ UEs
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Assign IP address to UEs
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Set the default gateway for the UEs
    Ptr<Node> ue = ueNode.Get(0);
    Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ue->GetObject<Ipv4>());
    ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);

    // Attach UE to eNB
    lteHelper->Attach(ueDevs.Get(0), enbDevs.Get(0));

    // Set up UDP server (sink) on eNB/remoteHost
    uint16_t sinkPort = 4000;
    Address sinkAddress(InetSocketAddress(remoteHostAddr, sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(remoteHost);
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    // Set up UDP client on UE, sending to remoteHost (eNB side) via core
    uint32_t maxPacketCount = 5;
    uint32_t packetSize = 512;
    Time interPacketInterval = Seconds(1.0);

    UdpClientHelper clientHelper(sinkAddress, sinkPort);
    clientHelper.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
    clientHelper.SetAttribute("Interval", TimeValue(interPacketInterval));
    clientHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps = clientHelper.Install(ueNode.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // Enable tracing (optional)
    lteHelper->EnableTraces();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}