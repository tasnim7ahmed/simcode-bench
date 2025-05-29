#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes: one gNodeB, one UE
    NodeContainer gNbNodes;
    gNbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Position the gNodeB statically
    Ptr<ListPositionAllocator> gnbPositionAlloc = CreateObject<ListPositionAllocator>();
    gnbPositionAlloc->Add(Vector(50.0, 50.0, 1.5)); // Centered, 1.5m height

    MobilityHelper gnbMobility;
    gnbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    gnbMobility.SetPositionAllocator(gnbPositionAlloc);
    gnbMobility.Install(gNbNodes);

    // UE Mobility: RandomWalk2dMobilityModel within 100x100 area
    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel(
        "ns3::RandomWalk2dMobilityModel",
        "Bounds", RectangleValue(Rectangle(0.0, 100.0, 0.0, 100.0)),
        "Distance", DoubleValue(10.0),
        "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    ueMobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
        "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
        "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    ueMobility.Install(ueNodes);

    // NR Helper
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();

    // Spectrum, propagation models for NR
    nrHelper->SetGnbAntennaModelType("ns3::IsotropicAntennaModel");

    // Set the channel and spectrum helpers
    Ptr<ThreeGppChannelModel> channelModel = CreateObject<ThreeGppChannelModel>();
    nrHelper->SetChannelModel(channelModel);

    // Core network
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    nrHelper->SetEpcHelper(epcHelper);

    // Install NR devices
    NetDeviceContainer gNbDevs = nrHelper->InstallGnbDevice(gNbNodes);
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice(ueNodes);

    // Attach UE to gNB
    nrHelper->AttachToClosestEnb(ueDevs, gNbDevs);

    // Activate default EPS bearer (eMBB Video -- for generic TCP)
    nrHelper->ActivateDataRadioBearer(ueDevs, NrHelper::DefaultEpsBearer(), EpcTft::Default());

    // Install IP stack
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Get UE and gNB IP interfaces
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));
    Ipv4Address ueIp = ueIpIfaces.GetAddress(0);

    // Assign static IP for gNodeB application, via EPC's SGW/PGW
    Ptr<Node> pgw = epcHelper->GetPgwNode();
    InternetStackHelper internetPgw;
    internetPgw.Install(pgw);

    // Set up server on gNB: connect server to EPC's PGW node, set up loopback interface
    uint16_t serverPort = 5000;

    Address serverAddress(InetSocketAddress(Ipv4Address::GetAny(), serverPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApps = packetSinkHelper.Install(pgw);
    serverApps.Start(Seconds(0.5));
    serverApps.Stop(Seconds(10.0));

    // Set up routing so packets from UEs to PGW reach the application
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> pgwStaticRouting = ipv4RoutingHelper.GetStaticRouting(pgw->GetObject<Ipv4>());
    pgwStaticRouting->AddNetworkRouteTo(ueIp, Ipv4Mask::GetOnes(), 1);

    // TCP client on UE
    OnOffHelper clientHelper("ns3::TcpSocketFactory", Address());
    std::ostringstream msgSizeStr;
    msgSizeStr << 512;
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(512));

    // Retrieve PGW's interface address for downlink
    Ipv4Address remoteAddr = epcHelper->GetUeDefaultGatewayAddress();

    AddressValue remoteAddress(InetSocketAddress(remoteAddr, serverPort));
    clientHelper.SetAttribute("Remote", remoteAddress);

    ApplicationContainer clientApps = clientHelper.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(6.0)); // Will only send for 5 intervals

    // Limit application to 5 packets (intervals = 1s)
    clientHelper.SetAttribute("MaxPackets", UintegerValue(5));
    clientHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));

    Simulator::Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}