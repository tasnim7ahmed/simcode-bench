#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/nr-module.h"
#include "ns3/point-to-point-epc-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer ueNodes;
    ueNodes.Create(1);

    NodeContainer gnbNodes;
    gnbNodes.Create(1);

    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();

    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    nrHelper->SetEpcHelper(epcHelper);

    nrHelper->SetAttribute("BeamformingMethod", StringValue("CellScan"));
    nrHelper->SetAttribute("Scheduler", StringValue("ns3::NrMacSchedulerTdmaRR"));
    
    // Core network node
    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Install mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 1.5));    // gNB position
    positionAlloc->Add(Vector(100.0, 0.0, 1.5));  // UE position
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gnbNodes);
    mobility.Install(ueNodes);

    // Install NR devices
    NetDeviceContainer gnbDevs = nrHelper->InstallGnbDevice(gnbNodes);
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice(ueNodes);

    // Attach UE to gNB
    nrHelper->AttachToClosestEnb(ueDevs, gnbDevs);

    // Assign IP address to UE
    InternetStackHelper internet;
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));
    epcHelper->AddDefaultBearer(ueDevs.Get(0), NrQosFlowStats::GBR_CONVERSATIONAL_0);
    
    // Install IP stack on gNB for application use (not control/data plane)
    internet.Install(gnbNodes);

    // Create a remote host to generate traffic (the gNB acts as remote host for this purpose)
    NodeContainer remoteHostContainer;
    remoteHostContainer.Add(gnbNodes.Get(0));
    InternetStackHelper internet2;
    internet2.Install(remoteHostContainer);

    // Set up a point-to-point link between remote host (gNB) and PGW
    NodeContainer p2pNodes = NodeContainer(gnbNodes.Get(0), pgw);
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(Seconds(0.001)));
    NetDeviceContainer p2pDevs = p2p.Install(p2pNodes);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("10.1.0.0", "255.255.255.0");
    Ipv4InterfaceContainer internetIfaces = ipv4h.Assign(p2pDevs);

    // Add static routes for remoteHost (gNB)
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(remoteHostContainer.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // UDP Server on UE
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Client on gNB (remoteHost)
    UdpClientHelper client(ueIpIface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = client.Install(remoteHostContainer.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable checksum
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}