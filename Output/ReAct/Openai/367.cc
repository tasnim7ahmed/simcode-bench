#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create LTE nodes: 1 eNodeB, 1 UE
    NodeContainer ueNodes;
    ueNodes.Create(1);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Install Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // eNodeB
    positionAlloc->Add(Vector(30.0, 0.0, 0.0)); // UE

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    // Create and install LTE devices to the nodes
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on the UE and remote host
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Create and configure remote host (connected to PGW)
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    internet.Install(remoteHostContainer);

    // Configure the internet between PGW and remote host
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));

    Ptr<Node> pgw = epcHelper->GetPgwNode();
    NodeContainer internetNodes;
    internetNodes.Add(pgw);
    internetNodes.Add(remoteHost);

    NetDeviceContainer internetDevices = p2ph.Install(internetNodes);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

    // Routing between remote host and LTE network
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Assign UEs IP address and attach to eNodeB
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs.Get(0)));

    // Set default gateway for UE
    Ptr<Node> ueNode = ueNodes.Get(0);
    Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNode->GetObject<Ipv4>());
    ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);

    // Install TCP server at eNodeB (acts as sink)
    uint16_t sinkPort = 50000;
    Address sinkAddress(InetSocketAddress(ueIpIface.GetAddress(0), sinkPort)); // actually UE
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(ueNodes.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    // Install TCP client at remote host (acts as source)
    OnOffHelper clientHelper("ns3::TcpSocketFactory", Address());
    clientHelper.SetAttribute("Remote", AddressValue(InetSocketAddress(ueIpIface.GetAddress(0), sinkPort)));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    clientHelper.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    clientHelper.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
    ApplicationContainer clientApps = clientHelper.Install(remoteHost);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}