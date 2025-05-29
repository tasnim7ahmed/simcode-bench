#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"
#include "ns3/point-to-point-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes: 1 gNB, 2 UEs
    NodeContainer gnbNodes;
    gnbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(2);

    // Install NR devices on nodes
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    nrHelper->SetGnbDeviceAttribute("TxPower", DoubleValue(30.0));
    nrHelper->SetUeDeviceAttribute("TxPower", DoubleValue(23.0));

    // Set EPC helper and core network
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    nrHelper->SetEpcHelper(epcHelper);

    // Spectrum configuration (default band 1, 100 MHz)
    nrHelper->InitializeOperationBand(1, BandwidthPartInfo::SingleBandwidthPart(1, 100000000, 6350000000, 0, 0, 0));
    nrHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(false));
    nrHelper->SetPathlossModelAttribute("Frequency", DoubleValue(6350000000));

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(gnbNodes);

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue("ns3::UniformRandomVariable[Min=0|Max=50]"),
                                 "Y", StringValue("ns3::UniformRandomVariable[Min=0|Max=50]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Mode", StringValue("Time"),
                              "Time", StringValue("2s"),
                              "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=2.0]"),
                              "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
    mobility.Install(ueNodes);

    MobilityHelper gnbMobility;
    gnbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    gnbMobility.Install(gnbNodes);

    // Create NR devices
    NetDeviceContainer gnbDevs = nrHelper->InstallGnbDevice(gnbNodes);
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice(ueNodes);

    // Attach UEs to gNB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        nrHelper->Attach(ueDevs.Get(i), gnbDevs.Get(0));
    }

    // Assign IP addresses
    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Set default gateway for UEs
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<Node> ueNode = ueNodes.Get(i);
        Ipv4StaticRoutingHelper ipv4RoutingHelper;
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNode->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Application: TCP (BulkSend on UE 0 as client, PacketSink on UE 1 as server)
    uint16_t port = 8080;
    Address sinkAddress(InetSocketAddress(ueIpIfaces.GetAddress(1), port));

    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(ueNodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    BulkSendHelper clientHelper("ns3::TcpSocketFactory", sinkAddress);
    clientHelper.SetAttribute("MaxBytes", UintegerValue(0));
    clientHelper.SetAttribute("SendSize", UintegerValue(1024));
    ApplicationContainer clientApps = clientHelper.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // Set TCP variant (CUBIC by default)
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpCubic::GetTypeId()));

    // Enable checksum
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));
    
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}