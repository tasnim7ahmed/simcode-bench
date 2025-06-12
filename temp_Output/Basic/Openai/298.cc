#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/epc-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    // 1. Create eNodeBs, UE, remote host
    NodeContainer enbNodes;
    enbNodes.Create(2);
    NodeContainer ueNodes;
    ueNodes.Create(1);
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    // 2. Install Internet stack to remote party
    InternetStackHelper internet;
    internet.Install(remoteHost);

    // 3. EPC & LTE helpers
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // 4. Point2Point link between remote host and PGW
    Ptr<Node> pgw = epcHelper->GetPgwNode();
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    // interface 0 is pgw, 1 is remoteHost
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

    // 5. Route on remoteHost
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = 
        ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(
        Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // 6. Set eNodeB positions
    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue(0.0),
                                     "MinY", DoubleValue(0.0),
                                     "DeltaX", DoubleValue(100.0),
                                     "DeltaY", DoubleValue(0.0),
                                     "GridWidth", UintegerValue(2),
                                     "LayoutType", StringValue("RowFirst"));
    enbMobility.Install(enbNodes);

    // 7. UE mobility: random walk between eNodeBs
    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0, 100, -10, 10)),
                                "Time", TimeValue(Seconds(1.0)),
                                "Speed", StringValue("ns3::ConstantRandomVariable[Constant=8.0]"));
    ueMobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                    "MinX", DoubleValue(0.0),
                                    "MinY", DoubleValue(0.0),
                                    "DeltaX", DoubleValue(0.0),
                                    "GridWidth", UintegerValue(1),
                                    "LayoutType", StringValue("RowFirst"));
    ueMobility.Install(ueNodes);

    // 8. Install LTE Devices to the nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // 9. Install IP stack on UE
    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));
    Ptr<Node> ueNode = ueNodes.Get(0);

    // 10. Attach UE to eNodeB[0]
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0)->GetObject<LteEnbNetDevice>()->GetCellId());

    // 11. Activate dedicated EPS bearer for internet traffic flow template
    enum EpsBearer::Qci q = EpsBearer::GBR_CONV_VOICE;
    EpsBearer bearer(q);
    lteHelper->ActivateDedicatedEpsBearer(ueLteDevs, bearer, EpcTft::Default());

    // 12. UDP server on remote host (as the correspondent node behind the EPC)
    uint16_t port = 9000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(remoteHost);
    serverApps.Start(Seconds(0.1));
    serverApps.Stop(Seconds(10.0));

    // 13. UDP client on UE
    UdpClientHelper udpClient(remoteHostAddr, port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(3200));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = udpClient.Install(ueNode);
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // 14. Enable handover (default algorithm)
    lteHelper->AddX2Interface(enbNodes);

    // Optionally: enable traces
    //lteHelper->EnableTraces();

    // 15. Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}