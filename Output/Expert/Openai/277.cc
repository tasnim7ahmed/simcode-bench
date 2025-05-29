#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-interface-container.h"
#include "ns3/nr-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    uint32_t numEnb = 1;
    uint32_t numUe = 1;

    // Create nodes
    NodeContainer gnbNodes;
    gnbNodes.Create(numEnb);
    NodeContainer ueNodes;
    ueNodes.Create(numUe);

    // Install Mobility
    MobilityHelper gnbMobility;
    gnbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    gnbMobility.SetPositionAllocator("ns3::ListPositionAllocator",
                                    "PositionList", VectorValue({Vector(50.0, 50.0, 1.5)}));
    gnbMobility.Install(gnbNodes);

    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0.0, 100.0, 0.0, 100.0)),
                                "Distance", DoubleValue(20.0),
                                "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"));
    ueMobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                    "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                    "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    ueMobility.Install(ueNodes);

    // Install NR components
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    nrHelper->SetAttribute("UseIdealRrc", BooleanValue(true));

    // Spectrum channel and forwarding
    Ptr<NrPointToPointEpcHelper> epcHelper = CreateObject<NrPointToPointEpcHelper>();
    nrHelper->SetEpcHelper(epcHelper);
    nrHelper->SetGnbDeviceAttribute("AntennaNum", UintegerValue(4));
    nrHelper->SetUeDeviceAttribute("AntennaNum", UintegerValue(2));
    nrHelper->SetGnbAntennaAttribute("AntennaModel", StringValue("ns3::IsotropicAntennaModel"));

    // Configuration of core network
    Ptr<Node> pgw = epcHelper->GetPgwNode();

    InternetStackHelper internet;
    internet.Install(gnbNodes);
    internet.Install(ueNodes);

    // NR devices
    NetDeviceContainer gnbDevs = nrHelper->InstallGnbDevice(gnbNodes, {28e9}, {100e6}, {1});
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice(ueNodes, {28e9}, {100e6}, {1});

    nrHelper->AttachToClosestEnb(ueDevs, gnbDevs);

    // IP configuration
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));
    epcHelper->AddDefaultGateway(ueNodes.Get(0), Ipv4Address("7.0.0.1"));

    // Create a remote host (for TCP server)
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internetHost;
    internetHost.Install(remoteHost);

    // Set up Internet via P2P between PGW and 'remote host'
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    p2ph.SetChannelAttribute("Delay", StringValue("1ms"));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIfaces.GetAddress(1);

    // Routing
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Assign IP to gNodeB for TCP server
    Ipv4InterfaceContainer gnbIfaces;
    NetDeviceContainer gnbDeviceNet;
    gnbDeviceNet.Add(gnbNodes.Get(0)->GetDevice(0));
    gnbIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(gnbDeviceNet));

    // Start TCP server (sink) on gNodeB
    uint16_t port = 50000;
    Address sinkLocalAddress(InetSocketAddress(gnbIfaces.GetAddress(0), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(gnbNodes.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Create TCP client on UE to send packets to TCP server
    OnOffHelper clientHelper("ns3::TcpSocketFactory", Address());
    clientHelper.SetAttribute("Remote", AddressValue(sinkLocalAddress));
    clientHelper.SetAttribute("PacketSize", UintegerValue(512));
    clientHelper.SetAttribute("MaxBytes", UintegerValue(5 * 512));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("4096bps"))); // Just enough for required pacing
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));

    ApplicationContainer clientApp = clientHelper.Install(ueNodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}