#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/nr-module.h"
#include "ns3/nr-helper.h"
#include "ns3/nr-point-to-point-epc-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging if needed
    // LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create nodes: gNB and UE
    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Create EPC and NR Helper
    Ptr<NrPointToPointEpcHelper> epcHelper = CreateObject<NrPointToPointEpcHelper>();
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();

    nrHelper->SetEpcHelper(epcHelper);

    // Spectrum configuration
    nrHelper->SetChannelBandwidth (28e9, 100e6, 1); // 28 GHz, 100 MHz bw, 1 component carrier
    nrHelper->InitializeOperationBand (28e9, 100e6); // Single operational band

    // GNB device creation
    NetDeviceContainer enbDevs = nrHelper->InstallGnbDevice (enbNodes);

    // UE device creation
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice (ueNodes);

    // Mobility
    // gNB static at center (50, 50)
    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
    enbPositionAlloc->Add(Vector(50.0, 50.0, 1.5));
    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    enbMobility.SetPositionAllocator(enbPositionAlloc);
    enbMobility.Install(enbNodes);

    // UE random walk in 100x100
    MobilityHelper ueMobility;
    ueMobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
        "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
        "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]")
    );
    ueMobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                 "Bounds", RectangleValue(Rectangle (0, 100, 0, 100)),
                                 "Distance", DoubleValue(10.0),
                                 "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"));
    ueMobility.Install(ueNodes);

    // Internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    // Assign IP addresses to UE devices
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address (NetDeviceContainer(ueDevs));

    // Attach UE to eNB
    nrHelper->AttachToClosestEnb (ueDevs, enbDevs);

    // Add default route for UE
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        Ptr<Node> ue = ueNodes.Get(u);
        Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting>(ue->GetObject<Ipv4>()->GetRoutingProtocol ());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

    // Create a remote host (for EPC core point-to-point link)
    Ptr<Node> remoteHost = CreateObject<Node> ();
    NodeContainer remoteHostContainer;
    remoteHostContainer.Add(remoteHost);
    InternetStackHelper internetHost;
    internetHost.Install(remoteHostContainer);

    // Create point-to-point link from PGW to remote host (for TCP server)
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gbps")));
    p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.001)));
    NetDeviceContainer internetDevices = p2ph.Install(epcHelper->GetPgwNode(), remoteHost);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIfaces.GetAddress(1);

    // Static routing on remoteHost
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting>(remoteHost->GetObject<Ipv4>()->GetRoutingProtocol ());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // TCP server (PacketSink) on remoteHost (will be reached via gNB/EPC from UE)
    uint16_t sinkPort = 50000;
    Address sinkAddress (InetSocketAddress (remoteHostAddr, sinkPort));
    PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(remoteHost);
    sinkApps.Start (Seconds (0.0));
    sinkApps.Stop (Seconds (10.0));

    // TCP client on UE
    ApplicationContainer clientApps;
    OnOffHelper clientHelper ("ns3::TcpSocketFactory", sinkAddress);
    clientHelper.SetAttribute ("PacketSize", UintegerValue (512));
    clientHelper.SetAttribute ("MaxBytes", UintegerValue (512*5));
    clientHelper.SetAttribute ("DataRate", StringValue ("2Mbps"));
    clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

    Ptr<OnOffApplication> app = DynamicCast<OnOffApplication>(clientHelper.Install(ueNodes.Get(0)).Get(0));
    app->SetAttribute ("Interval", TimeValue (Seconds (1.0)));

    clientApps.Add(app);
    clientApps.Start (Seconds (1.0));
    clientApps.Stop (Seconds (10.0));

    Simulator::Stop (Seconds (10.0));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}