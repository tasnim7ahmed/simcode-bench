#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes: eNB and UE
    NodeContainer ueNodes;
    ueNodes.Create(1);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Mobility
    MobilityHelper mobility;

    // eNB position (fixed)
    Ptr<ListPositionAllocator> enbPosAlloc = CreateObject<ListPositionAllocator>();
    enbPosAlloc->Add(Vector(0.0, 0.0, 0.0));
    mobility.SetPositionAllocator(enbPosAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);

    // UE random walk
    Ptr<ListPositionAllocator> uePosAlloc = CreateObject<ListPositionAllocator>();
    uePosAlloc->Add(Vector(10.0, 0.0, 0.0));
    mobility.SetPositionAllocator(uePosAlloc);
    mobility.SetMobilityModel(
        "ns3::RandomWalk2dMobilityModel",
        "Mode", StringValue("Time"),
        "Time", TimeValue(Seconds(1.0)),
        "Speed", StringValue("ns3::ConstantRandomVariable[Constant=3.0]"),
        "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50))
    );
    mobility.Install(ueNodes);

    // LTE setup
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    // Use default EPC helper (includes PGW etc.)
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Install LTE devices to the nodes
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on the UE
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP addresses to UEs, retrieve PGW
    Ipv4InterfaceContainer ueIpIfaces;
    Ptr<Node> pgw = epcHelper->GetPgwNode();
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UE to eNB
    lteHelper->Attach(ueDevs.Get(0), enbDevs.Get(0));

    // Set default route for UE to the gateway
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> ueStaticRouting = 
        ipv4RoutingHelper.GetStaticRouting(ueNodes.Get(0)->GetObject<Ipv4>());
    ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);

    // Install UDP server on eNB
    // Need IP for eNB: connect a remote host via point-to-point link to PGW and route eNB <-> remote host traffic via LTE EPC
    // But since the eNB is not an IP node, we need a remote host attached to EPC for the UE to send traffic to

    // Add remote host
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    // Set up link from remote host to PGW
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    NetDeviceContainer internetDevices = p2ph.Install(remoteHost, pgw);

    // Install internet stack on remote host
    internet.Install(remoteHostContainer);

    // Assign IP addresses to remote host-epc link
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer remoteHostIf = ipv4h.Assign(internetDevices);

    // Add static route on remote host (to reach UE subnet via PGW)
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(
        Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // UDP server on remoteHost
    uint16_t udpServerPort = 1234;
    UdpServerHelper udpServer(udpServerPort);
    ApplicationContainer serverApps = udpServer.Install(remoteHost);
    serverApps.Start(Seconds(0.1));
    serverApps.Stop(Seconds(10.0));

    // UDP client on UE (send to remoteHost's interface)
    UdpClientHelper udpClient(remoteHostIf.GetAddress(0), udpServerPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(320));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.03)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(9.0));

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}