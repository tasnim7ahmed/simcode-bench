#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

#include "ns3/point-to-point-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/netanim-module.h"

#include "ns3/antenna-module.h"
#include "ns3/antenna-module.h"

// NS-3.41+ 5G modules:
#include "ns3/nr-module.h"
#include "ns3/epc-helper.h"
#include "ns3/internet-apps-module.h"

using namespace ns3;

int main(int argc, char* argv[])
{
    // Logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Simulation parameters
    uint16_t numUes = 2;
    double simTime = 10.0;

    // Create nodes: 1 gNB + 2 UEs
    NodeContainer gNbNodes;
    gNbNodes.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(numUes);

    // Mobility for gNBs (stationary)
    MobilityHelper gNbMobility;
    gNbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    gNbMobility.Install(gNbNodes);

    Ptr<ListPositionAllocator> uePositionAlloc = CreateObject<ListPositionAllocator>();
    uePositionAlloc->Add(Vector(50.0, 0.0, 1.5));  // UE1 start position
    uePositionAlloc->Add(Vector(60.0, 0.0, 1.5));  // UE2 start position

    // Mobility for UEs: RandomWalk2d
    MobilityHelper ueMobility;
    ueMobility.SetPositionAllocator(uePositionAlloc);
    ueMobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                 "Bounds", RectangleValue (Rectangle (0, 100, -50, 50)),
                                 "Distance", DoubleValue (20.0),
                                 "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=2.0]"),
                                 "Time", TimeValue (Seconds (1.0)));
    ueMobility.Install(ueNodes);

    // Install NR and EPC
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    nrHelper->SetAttribute("BeamformingMethod", StringValue("DIRECT_PATH"));

    // EPC/Core network
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    nrHelper->SetEpcHelper(epcHelper);

    // Spectrum configuration
    nrHelper->SetAttribute("OperationBand", StringValue("mmwave"));

    // Bandwidth part configuration
    nrHelper->SetGnbPhyAttribute("Numerology", UintegerValue(2)); // 120 kHz SCS (mmWave), can be adjusted
    nrHelper->SetGnbPhyAttribute("Frequency", DoubleValue(28e9));

    // Configure one sector per gNB
    NetDeviceContainer gNbDevs = nrHelper->InstallGnbDevice(gNbNodes);
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice(ueNodes);

    // Attach UEs to gNB (all UEs attached to only gNB)
    for (uint32_t i = 0; i < ueDevs.GetN(); ++i)
    {
        nrHelper->Attach(ueDevs.Get(i), gNbDevs.Get(0));
    }

    // Activate default data radio bearer
    enum EpsBearer::Qci q = EpsBearer::NGBR_VIDEO_TCP_DEFAULT;
    EpsBearer bearer(q);
    for (uint32_t i = 0; i < ueDevs.GetN(); ++i)
    {
        nrHelper->ActivateDedicatedEpsBearer(ueDevs.Get(i), bearer, EpcTft::Default());
    }

    // Internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Get PGW and assign IPs
    Ipv4InterfaceContainer ueIpIfaces;
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("192.168.1.0", "255.255.255.0");

    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Create a remote host connected to the PGW (not necessary here, but required for NS-3 EPC)
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue (DataRate ("100Gb/s")));
    p2ph.SetChannelAttribute("Delay", TimeValue (Seconds(0.010)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

    InternetStackHelper internetHostHelper;
    internetHostHelper.Install(remoteHost);

    Ipv4AddressHelper ipv4h2;
    ipv4h2.SetBase("10.1.0.0", "255.255.255.0");
    Ipv4InterfaceContainer hostInterfaces = ipv4h2.Assign(internetDevices);

    // Add static route for remotehost (not strictly necessary in this simulation)
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo("192.168.1.0", "255.255.255.0", 1);

    // Assign IP addresses to UEs
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Set UEs' default gateway to EPC
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNodes.Get(i)->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Application: UDP Echo Server on UE2
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(ueNodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    // UDP Echo Client on UE1, targets UE2
    UdpEchoClientHelper echoClient(ueIpIfaces.GetAddress(1), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}