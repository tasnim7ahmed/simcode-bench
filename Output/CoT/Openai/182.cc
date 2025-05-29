#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create Nodes: 2 UEs, 1 eNodeB, 1 EPC gateway (created by EPC)
    NodeContainer ueNodes;
    ueNodes.Create(2);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Install mobility model
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));      // eNodeB position
    positionAlloc->Add(Vector(50.0, 0.0, 0.0));     // UE 0 position
    positionAlloc->Add(Vector(100.0, 0.0, 0.0));    // UE 1 position
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    // Create LTE helper and EPC helper
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Install eNodeB Devices to the eNodeB nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    // Install UE Devices to the UE nodes
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the Internet stack on the UEs
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP address to UEs, get SGW/PGW
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Add a remote host
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internetHost;
    internetHost.Install(remoteHost);
    // PointToPoint link between RemoteHost and PGW
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    // Assign remoteHost IPv4 address
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

    // Add static routes
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Attach UEs to the eNodeB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
        lteHelper->Attach(ueLteDevs.Get(i), enbLteDevs.Get(0));

    // Set up default gateway for UEs
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNodes.Get(i)->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // TCP applications: UE0 (client) <-> UE1 (server)
    // Since UEs are on LTE network with EPC, they have IPs
    // We'll use TCP bulk send for data transfer
    uint16_t tcpPort = 50000;

    // Install sink on UE1
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(ueNodes.Get(1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(8.0));

    // Install TCP source on UE0 (send to UE1)
    BulkSendHelper bulkSender("ns3::TcpSocketFactory", InetSocketAddress(ueIpIface.GetAddress(1), tcpPort));
    bulkSender.SetAttribute("MaxBytes", UintegerValue(1000000));
    ApplicationContainer sourceApps = bulkSender.Install(ueNodes.Get(0));
    sourceApps.Start(Seconds(2.0));
    sourceApps.Stop(Seconds(8.0));

    // Enable tracing in NetAnim
    AnimationInterface anim("lte-two-ue.xml");
    anim.SetConstantPosition(enbNodes.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(ueNodes.Get(0), 50.0, 0.0);
    anim.SetConstantPosition(ueNodes.Get(1), 100.0, 0.0);
    anim.SetConstantPosition(remoteHost, -30.0, 0.0);

    // Set descriptions for easier visualization
    anim.UpdateNodeDescription(enbNodes.Get(0), "eNodeB");
    anim.UpdateNodeDescription(ueNodes.Get(0), "UE0");
    anim.UpdateNodeDescription(ueNodes.Get(1), "UE1");
    anim.UpdateNodeDescription(remoteHost, "RemoteHost");

    Simulator::Stop(Seconds(9.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}