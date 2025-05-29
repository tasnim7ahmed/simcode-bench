#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/nr-module.h"
#include "ns3/antenna-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes: 1 gNB, 1 UE, 1 remote host
    NodeContainer ueNodes, gnbNodes, remoteHostContainer;
    ueNodes.Create(1);
    gnbNodes.Create(1);
    remoteHostContainer.Create(1);

    Ptr<Node> ue = ueNodes.Get(0);
    Ptr<Node> gnb = gnbNodes.Get(0);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    // Mobility: gNodeB static at (50,50), UE random walk in 100x100
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
    posAlloc->Add(Vector(50.0, 50.0, 1.5));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.SetPositionAllocator(posAlloc);
    mobility.Install(gnbNodes);

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue("Time"),
                             "Time", StringValue("2s"),
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                             "Bounds", RectangleValue(Rectangle(0.0, 100.0, 0.0, 100.0)));
    mobility.Install(ueNodes);

    // NR Helper
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();

    // Set propagation model and spectrum
    nrHelper->SetAttribute("Scheduler", StringValue("ns3::NrMacSchedulerTdmaRR"));
    Ptr<ThreeGppChannelModel> channelModel = CreateObject<ThreeGppChannelModel>();
    nrHelper->SetChannel(channelModel);

    // gNB configuration
    NetDeviceContainer gnbDevs = nrHelper->InstallGnbDevice(gnbNodes, 1);

    // UE configuration
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice(ueNodes, 1);

    // Antennas
    nrHelper->SetGnbAntennaAttribute("NumRows", UintegerValue(1));
    nrHelper->SetGnbAntennaAttribute("NumColumns", UintegerValue(4));
    nrHelper->SetGnbAntennaAttribute("AntennaElement", PointerValue(CreateObject<IsotropicAntennaModel>()));
    nrHelper->SetUeAntennaAttribute("NumRows", UintegerValue(1));
    nrHelper->SetUeAntennaAttribute("NumColumns", UintegerValue(1));

    // Core network with point-to-point link to remote host
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer internetDevs = p2p.Install(gnbNodes.Get(0), remoteHost);

    // Install the Internet stack on all nodes
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);
    internet.Install(gnbNodes);
    internet.Install(ueNodes);

    // Addressing
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer rhsIfaces = ipv4h.Assign(internetDevs);

    Ipv4Address remoteHostAddr = rhsIfaces.GetAddress(1);

    nrHelper->AssignIpv4Addresses(ueDevs, ueNodes);
    nrHelper->AssignIpv4Addresses(gnbDevs, gnbNodes);

    // Attach UE to the closest gNB
    for (uint32_t i = 0; i < ueDevs.GetN(); ++i)
    {
        nrHelper->Attach(ueDevs.Get(i), gnbDevs.Get(0));
    }

    // Set up routing
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
        ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    Ipv4InterfaceContainer ueIfaces = nrHelper->GetUeIpv4Interface(ueDevs.Get(0));
    Ipv4InterfaceContainer gnbIfaces = nrHelper->GetGnbIpv4Interface(gnbDevs.Get(0));

    // TCP Server on gNB
    uint16_t sinkPort = 5000;
    Address sinkAddress(InetSocketAddress(gnbIfaces.GetAddress(0), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApps = packetSinkHelper.Install(gnb);
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    // TCP Client on UE
    OnOffHelper clientHelper("ns3::TcpSocketFactory", sinkAddress);
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("4Mbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(512));
    clientHelper.SetAttribute("MaxBytes", UintegerValue(512 * 5));
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=0.1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer clientApps = clientHelper.Install(ue);
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}