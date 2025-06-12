#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging if needed
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Parameters
    uint16_t numberOfUes = 2;
    uint16_t numberOfEnb = 1;
    double simTime = 5.0; // seconds
    uint16_t dlPort = 1100;
    uint32_t packetSize = 1024;
    double interPacketInterval = 0.01; // seconds

    // Create LTE elements
    NodeContainer ueNodes;
    ueNodes.Create(numberOfUes);
    NodeContainer enbNodes;
    enbNodes.Create(numberOfEnb);

    // Create a single remote host
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    // Install internet stack on remote host
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // Create point-to-point link between PGW and remote host
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gbps")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.01)));

    // LTE helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Get PGW node
    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Create P2P devices and channels between remote host and PGW
    NodeContainer p2pNodes;
    p2pNodes.Add(pgw);
    p2pNodes.Add(remoteHost);

    NetDeviceContainer internetDevices = p2ph.Install(p2pNodes);

    // Assign IP to remote host
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

    // Set up routing for remote host
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo("7.0.0.0", "255.0.0.0", 1);

    // Install mobility for eNodeB and UEs
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
    enbPositionAlloc->Add(Vector(0.0, 0.0, 0.0));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.SetPositionAllocator(enbPositionAlloc);
    mobility.Install(enbNodes);

    Ptr<ListPositionAllocator> uePositionAlloc = CreateObject<ListPositionAllocator>();
    uePositionAlloc->Add(Vector(50.0, 0.0, 0.0));
    uePositionAlloc->Add(Vector(100.0, 0.0, 0.0));
    mobility.SetPositionAllocator(uePositionAlloc);
    mobility.Install(ueNodes);

    // Install LTE devices to the nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install IP stack on UEs
    internet.Install(ueNodes);

    // Assign IP addresses to UEs and attach
    Ipv4InterfaceContainer ueIpIfaces;
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        Ptr<Node> ueNode = ueNodes.Get(u);
        ueIpIfaces.Add(epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs.Get(u))));
        lteHelper->Attach(ueLteDevs.Get(u), enbLteDevs.Get(0));
        // Set up default route on the UE
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNode->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Install and start UDP server on remote host
    UdpServerHelper udpServer(dlPort);
    ApplicationContainer serverApps = udpServer.Install(remoteHost);
    serverApps.Start(Seconds(0.01));
    serverApps.Stop(Seconds(simTime));

    // Install UDP client on each UE
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        UdpClientHelper udpClient(remoteHostAddr, dlPort);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000000));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(interPacketInterval)));
        udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));
        ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(u));
        clientApps.Start(Seconds(0.5 + 0.1 * u));
        clientApps.Stop(Seconds(simTime));
    }

    // NetAnim Setup
    AnimationInterface anim("lte-simple-netanim.xml");
    for (uint32_t i = 0; i < enbNodes.GetN(); ++i)
    {
        anim.UpdateNodeDescription(enbNodes.Get(i), "eNodeB");
        anim.UpdateNodeColor(enbNodes.Get(i), 0, 255, 0);
    }
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        anim.UpdateNodeDescription(ueNodes.Get(i), "UE" + std::to_string(i + 1));
        anim.UpdateNodeColor(ueNodes.Get(i), 0, 0, 255);
    }
    anim.UpdateNodeDescription(remoteHost, "RemoteHost");
    anim.UpdateNodeColor(remoteHost, 255, 0, 0);

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}