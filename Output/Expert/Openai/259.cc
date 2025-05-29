#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/nr-module.h"
#include "ns3/mmwave-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Configure NR module attributes
    Config::SetDefault("ns3::NrHelper::UlSched", StringValue("Tdma"));
    Config::SetDefault("ns3::NrHelper::DlSched", StringValue("Tdma"));

    // Create Node Containers
    NodeContainer gNbNodes;
    gNbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(2);

    // Mobility for gNB: stationary at (0,0,4)
    Ptr<ListPositionAllocator> gNbPositionAlloc = CreateObject<ListPositionAllocator>();
    gNbPositionAlloc->Add(Vector(0.0, 0.0, 4.0));
    MobilityHelper gNbMobility;
    gNbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    gNbMobility.SetPositionAllocator(gNbPositionAlloc);
    gNbMobility.Install(gNbNodes);

    // Mobility for UEs: RandomWalk2d
    MobilityHelper ueMobility;
    ueMobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
        "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
        "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
        "Bounds", RectangleValue(Rectangle(0.0, 100.0, 0.0, 100.0)),
        "Distance", DoubleValue(10.0),
        "Speed", StringValue("ns3::ConstantRandomVariable[Constant=3.0]"),
        "Time", TimeValue(Seconds(1.0)));
    ueMobility.Install(ueNodes);

    // Install NR Devices
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    nrHelper->SetAttribute("BeamformingMethod", StringValue("CellScan"));
    nrHelper->SetAttribute("Scheduler", StringValue("PfFfMacScheduler"));
    Ptr<NrPointToPointEpcHelper> epcHelper = CreateObject<NrPointToPointEpcHelper>();
    nrHelper->SetEpcHelper(epcHelper);
    Ptr<IdealBeamformingHelper> beamHelper = CreateObject<IdealBeamformingHelper>();
    nrHelper->SetBeamformingHelper(beamHelper);

    // Spectrum configuration (simple 28GHz mmWave, 100MHz BW)
    nrHelper->SetGnbDeviceAttribute("DlEarfcn", UintegerValue(620000));
    nrHelper->SetGnbDeviceAttribute("UlEarfcn", UintegerValue(684000));
    nrHelper->SetGnbDeviceAttribute("DlBandwidth", UintegerValue(100));
    nrHelper->SetGnbDeviceAttribute("UlBandwidth", UintegerValue(100));

    NetDeviceContainer gnbDevs = nrHelper->InstallGnbDevice(gNbNodes);
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice(ueNodes);

    // Attach UE devices to gNb
    nrHelper->AttachToClosestEnb(ueDevs, gnbDevs);

    // Install internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(epcHelper->GetPgwNode());

    // Assign IPv4 addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ueIfaces = address.Assign(ueDevs);

    // UDP Echo applications: UE1 as client, UE2 as server
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApp = echoServer.Install(ueNodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(ueIfaces.GetAddress(1), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = echoClient.Install(ueNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}