#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteSimulationExample");

int main(int argc, char *argv[])
{
    // Set the simulation time
    double simulationTime = 10.0;

    // Create LTE nodes: 1 eNodeB (base station) and 2 UEs (User Equipments)
    NodeContainer ueNodes;
    ueNodes.Create(2);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Set up the LTE modules
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Create a remote host to act as the internet backbone
    Ptr<Node> pgw = epcHelper->GetPgwNode();
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // Set up a point-to-point link between the PGW and the remote host
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2p.SetChannelAttribute("Delay", TimeValue(Seconds(0.01)));
    NetDeviceContainer internetDevices = p2p.Install(pgw, remoteHost);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetInterfaces = ipv4.Assign(internetDevices);

    // Assign IP addresses to the remote host
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Install LTE devices to UEs and eNodeB
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install IP stack on the UEs
    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpInterfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Attach UEs to the eNodeB
    lteHelper->Attach(ueLteDevs, enbLteDevs.Get(0));

    // Set mobility for UEs and eNodeB
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    // Set positions for eNodeB and UEs
    Ptr<ConstantPositionMobilityModel> enbMobility = enbNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
    enbMobility->SetPosition(Vector(0.0, 0.0, 0.0)); // eNodeB at origin

    Ptr<ConstantPositionMobilityModel> ueMobility1 = ueNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
    ueMobility1->SetPosition(Vector(10.0, 0.0, 0.0)); // UE 1 at 10 meters away from eNodeB

    Ptr<ConstantPositionMobilityModel> ueMobility2 = ueNodes.Get(1)->GetObject<ConstantPositionMobilityModel>();
    ueMobility2->SetPosition(Vector(20.0, 0.0, 0.0)); // UE 2 at 20 meters away from eNodeB

    // Set up applications to send and receive data
    uint16_t dlPort = 1234;
    OnOffHelper onoff("ns3::TcpSocketFactory", Address(InetSocketAddress(ueIpInterfaces.GetAddress(1), dlPort)));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("DataRate", StringValue("50Mbps"));
    ApplicationContainer clientApps = onoff.Install(remoteHost);
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime));

    PacketSinkHelper sink("ns3::TcpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), dlPort)));
    ApplicationContainer serverApps = sink.Install(ueNodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));

    // Enable NetAnim
    AnimationInterface anim("lte_netanim.xml");

    // Set positions in NetAnim
    anim.SetConstantPosition(enbNodes.Get(0), 0.0, 0.0);   // eNodeB at origin
    anim.SetConstantPosition(ueNodes.Get(0), 10.0, 0.0);   // UE 1 at 10m
    anim.SetConstantPosition(ueNodes.Get(1), 20.0, 0.0);   // UE 2 at 20m

    anim.EnablePacketMetadata(true);  // Enable packet tracking in NetAnim

    // Run the simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

