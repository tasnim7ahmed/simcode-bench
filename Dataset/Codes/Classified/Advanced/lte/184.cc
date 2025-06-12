#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteSimulationExample");

int main(int argc, char *argv[])
{
    // Set simulation time
    double simulationTime = 10.0;

    // Create LTE Helper and EPC Helper for LTE core
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Create nodes: 1 eNodeB, 2 UEs (User Equipments), and 1 Remote Host
    NodeContainer ueNodes;
    ueNodes.Create(2);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Create a remote host
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // Configure the remote host to use a point-to-point link with the PGW (Packet Gateway)
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.01)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);

    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

    // Set up the routing between UEs and the remote host
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Install the LTE stack on UEs and eNodeB
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Set up mobility model for eNodeB and UEs
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    // Install the IP stack on UEs
    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Set up default gateways for UEs
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNodes.Get(i)->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Attach UEs to the eNodeB
    lteHelper->Attach(ueLteDevs, enbLteDevs.Get(0));

    // Install UDP traffic applications on UEs
    uint16_t dlPort = 1234;
    uint16_t ulPort = 2000;

    UdpClientHelper dlClient(remoteHostAddr, dlPort);
    dlClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    dlClient.SetAttribute("MaxPackets", UintegerValue(1000000));
    dlClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    clientApps.Add(dlClient.Install(ueNodes.Get(0)));
    clientApps.Add(dlClient.Install(ueNodes.Get(1)));

    UdpServerHelper ulServer(ulPort);
    ApplicationContainer serverApps = ulServer.Install(remoteHost);

    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime));

    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));

    // Enable NetAnim for visualization
    AnimationInterface anim("lte_netanim.xml");
    anim.SetConstantPosition(enbNodes.Get(0), 50.0, 50.0); // eNodeB
    anim.SetConstantPosition(ueNodes.Get(0), 60.0, 50.0);  // UE 1
    anim.SetConstantPosition(ueNodes.Get(1), 70.0, 50.0);  // UE 2
    anim.EnablePacketMetadata(true);                       // Enable packet tracking

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

