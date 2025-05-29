#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"
#include "ns3/nr-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for applications
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

    // Create one gNodeB and one UE
    NodeContainer gNbNodes;
    gNbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Install Mobility for gNB (static)
    Ptr<ListPositionAllocator> gNbPositionAlloc = CreateObject<ListPositionAllocator>();
    gNbPositionAlloc->Add(Vector(50.0, 50.0, 0.0));
    MobilityHelper gNbMobility;
    gNbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    gNbMobility.SetPositionAllocator(gNbPositionAlloc);
    gNbMobility.Install(gNbNodes);

    // UE: Randomly placed in 100x100, RandomWalk mobility
    MobilityHelper ueMobility;
    ueMobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
          "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
          "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
          "Bounds", RectangleValue(Rectangle(0.0, 100.0, 0.0, 100.0)),
          "Distance", DoubleValue(10.0));
    ueMobility.Install(ueNodes);

    // Install NR stack
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    nrHelper->SetGnbDeviceAttribute("AntennaNum", UintegerValue(4));
    nrHelper->SetUeDeviceAttribute("AntennaNum", UintegerValue(1));

    // Core network via internet helpers (EPC)
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    nrHelper->SetEpcHelper(epcHelper);
    nrHelper->SetAttribute("UseIdealRrc", BooleanValue(true));

    // Spectrum and operation band
    Ptr<NrPointToPointEpcHelper> nrEpcHelper = DynamicCast<NrPointToPointEpcHelper>(epcHelper);
    Ptr<NrHelper> nr = nrHelper;
    BandwidthPartInfoPtrVector allBwps;
    nr->SetPathlossAttribute ("ShadowingEnabled", BooleanValue (false));
    double centralFrequency = 28e9;
    double bandwidth = 100e6;
    CcBwpCreator ccBwpCreator;
    BandwidthPartInfoPtrVector bwpVector = ccBwpCreator.CreateSingleCc (centralFrequency, bandwidth, BandwidthPartInfo::UMi_StreetCanyon);
    nr->InitializeOperationBand (&bwpVector);

    // Install devices
    NetDeviceContainer gnbDevs = nr->InstallGnbDevice(gNbNodes, bwpVector);
    NetDeviceContainer ueDevs = nr->InstallUeDevice(ueNodes, bwpVector);

    // Attach UE
    nr->AttachToClosestEnb(ueDevs, gnbDevs);

    // Internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // EPC: Assign IP addresses to UE
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Route default gateway for the UE
    Ptr<Node> ueNode = ueNodes.Get(0);
    Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(ueNode->GetObject<Ipv4>()->GetRoutingProtocol());
    ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);

    // Install internet stack on remote host (to simulate an external server)
    Ptr<Node> remoteHost = CreateObject<Node>();
    NodeContainer remoteHostContainer;
    remoteHostContainer.Add(remoteHost);
    InternetStackHelper internetHost;
    internetHost.Install(remoteHost);

    // Connect remote host to PGW
    Ptr<Node> pgw = epcHelper->GetPgwNode();
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Gbps")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

    // Assign IP to remote host
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer remoteIfaces = ipv4h.Assign(internetDevices);

    // Add static route on remote host
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(remoteHost->GetObject<Ipv4>()->GetRoutingProtocol());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // TCP server on gNB (runs on remote host)
    uint16_t port = 5000;
    Address sinkAddress(InetSocketAddress(remoteIfaces.GetAddress(1), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(remoteHost);
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // TCP client (OnOffApplication) on UE, send 5 packets of 512 bytes at 1 second intervals
    OnOffHelper clientHelper("ns3::TcpSocketFactory", sinkAddress);
    clientHelper.SetAttribute("PacketSize", UintegerValue(512));
    clientHelper.SetAttribute("MaxBytes", UintegerValue(512*5));
    clientHelper.SetAttribute("DataRate", StringValue("512kbps"));
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=0.1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.9]"));

    ApplicationContainer clientApps = clientHelper.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}