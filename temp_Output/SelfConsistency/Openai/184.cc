#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteSimpleNetAnimExample");

int main(int argc, char *argv[])
{
    // Enable logging if needed
    // LogComponentEnable("LteSimpleNetAnimExample", LOG_LEVEL_INFO);

    // Set simulation parameters
    uint16_t numUes = 2;
    double simTime = 5.0;

    // Create nodes
    NodeContainer ueNodes;
    ueNodes.Create(numUes);
    NodeContainer enbNode;
    enbNode.Create(1);
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);

    // Create the core network
    Ptr<Node> pgw;
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    // LTE components
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    lteHelper->SetAttribute("UseIdealRrc", BooleanValue(true));

    // Connect the remote host to PGW
    pgw = epcHelper->GetPgwNode();
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gbps")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

    // Install Internet Stack on remote host
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // Assign IP addresses
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer remoteHostIf = ipv4h.Assign(internetDevices);

    // Set up routing
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Install mobility for eNB and UEs
    MobilityHelper mobility;
    // eNB position
    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
    enbPositionAlloc->Add(Vector(50.0, 50.0, 0.0));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.SetPositionAllocator(enbPositionAlloc);
    mobility.Install(enbNode);

    // UEs positions
    Ptr<ListPositionAllocator> uePositionAlloc = CreateObject<ListPositionAllocator>();
    uePositionAlloc->Add(Vector(40.0, 55.0, 0.0));
    uePositionAlloc->Add(Vector(60.0, 45.0, 0.0));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.SetPositionAllocator(uePositionAlloc);
    mobility.Install(ueNodes);

    // Install LTE Devices to the nodes
    NetDeviceContainer enbLteDev = lteHelper->InstallEnbDevice(enbNode);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on UEs
    InternetStackHelper ueInternet;
    ueInternet.Install(ueNodes);

    // Assign IP address to UEs and attach to eNB
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        lteHelper->Attach(ueLteDevs.Get(i), enbLteDev.Get(0));
        // Enable default bearer for traffic
        enum EpsBearer::Qci qci = EpsBearer::NGBR_VIDEO_TCP_DEFAULT;
        EpsBearer bearer(qci);
        lteHelper->ActivateDedicatedEpsBearer(ueLteDevs.Get(i), bearer, EpcTft::Default());
    }

    // Install UDP traffic from each UE to remote host
    uint16_t dlPort = 10000;

    // Install UDP server on remote host (to receive data)
    UdpServerHelper udpServer(dlPort);
    ApplicationContainer serverApps = udpServer.Install(remoteHost);
    serverApps.Start(Seconds(0.1));
    serverApps.Stop(Seconds(simTime));

    // Configure UDP clients on UEs
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        UdpClientHelper udpClient(remoteHostIf.GetAddress(0), dlPort);
        udpClient.SetAttribute("MaxPackets", UintegerValue(10000));
        udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
        udpClient.SetAttribute("PacketSize", UintegerValue(200));
        ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(i));
        clientApps.Start(Seconds(0.5 + i * 0.05));
        clientApps.Stop(Seconds(simTime));
    }

    // NetAnim setup
    AnimationInterface anim("lte-netanim.xml");

    anim.SetConstantPosition(enbNode.Get(0), 50.0, 50.0, 0);
    anim.SetConstantPosition(ueNodes.Get(0), 40.0, 55.0, 0);
    anim.SetConstantPosition(ueNodes.Get(1), 60.0, 45.0, 0);
    anim.SetConstantPosition(remoteHost, 95.0, 50.0, 0);

    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        anim.UpdateNodeDescription(ueNodes.Get(i), "UE" + std::to_string(i+1)); // Optional: label UEs
        anim.UpdateNodeColor(ueNodes.Get(i), 0, 255, 0);
    }
    anim.UpdateNodeDescription(enbNode.Get(0), "eNodeB");
    anim.UpdateNodeColor(enbNode.Get(0), 0, 0, 255);
    anim.UpdateNodeDescription(remoteHost, "RemoteHost");
    anim.UpdateNodeColor(remoteHost, 255, 0, 0);

    // Enable packet metadata and NetAnim tracing
    lteHelper->EnableTraces();

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}