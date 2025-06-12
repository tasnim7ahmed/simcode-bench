#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create eNodeB and UE nodes
    NodeContainer enbNodes;
    enbNodes.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(2);

    // Create remote host node
    NodeContainer remoteHost;
    remoteHost.Create(1);

    // LTE helper setup
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Set pathloss model
    lteHelper->SetPathlossModelType(TypeId::FindByName("ns3::FriisSpectrumPropagationLossModel"));

    // Install LTE devices
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on UEs and remote host
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(remoteHost);

    // Assign IP addresses to UEs and get PGW address from EPC helper
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Attach UEs to eNodeB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        lteHelper->Attach(ueLteDevs.Get(i), enbLteDevs.Get(0));
    }

    // Setup routing for remote host
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost.Get(0)->GetObject<Ipv4>());
    remoteHostRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.255.255.0"), 1);

    // Point-to-point link between PGW and remote host
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.001)));

    NetDeviceContainer internetDevices = p2ph.Install(epcHelper->GetPgwNode(), remoteHost.Get(0));

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);

    // Mobility models
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(enbNodes);
    mobility.Install(ueNodes);
    mobility.Install(remoteHost);

    // Position allocation for animation
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));   // eNodeB
    positionAlloc->Add(Vector(10.0, 0.0, 0.0));  // UE1
    positionAlloc->Add(Vector(20.0, 0.0, 0.0));  // UE2
    positionAlloc->Add(Vector(30.0, 0.0, 0.0));  // Remote Host

    MobilityHelper mobilityAll;
    mobilityAll.SetPositionAllocator(positionAlloc);
    mobilityAll.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    NodeContainer allNodes = NodeContainer(enbNodes, ueNodes, remoteHost);
    mobilityAll.Install(allNodes);

    // Install UDP Echo Server on remote host
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(remoteHost.Get(0));
    serverApps.Start(Seconds(0.1));
    serverApps.Stop(Seconds(10.0));

    // Install UDP Echo Clients on UEs
    uint32_t packetSize = 1024;
    Time interPacketInterval = MilliSeconds(100);
    uint32_t maxPacketCount = 30;

    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        UdpEchoClientHelper echoClient(internetIpIfaces.GetAddress(1), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
        echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
        echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

        ApplicationContainer clientApps = echoClient.Install(ueNodes.Get(i));
        clientApps.Start(Seconds(1.0 + i * 0.5));
        clientApps.Stop(Seconds(10.0));
    }

    // Animation
    AnimationInterface anim("lte_simulation.xml");
    anim.SetMobilityPollInterval(Seconds(0.1));

    // Color nodes
    anim.UpdateNodeDescription(enbNodes.Get(0), "eNodeB");
    anim.UpdateNodeColor(enbNodes.Get(0), 255, 0, 0); // red

    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        std::string desc = "UE" + std::to_string(i + 1);
        anim.UpdateNodeDescription(ueNodes.Get(i), desc);
        anim.UpdateNodeColor(ueNodes.Get(i), 0, 0, 255); // blue
    }

    anim.UpdateNodeDescription(remoteHost.Get(0), "RemoteHost");
    anim.UpdateNodeColor(remoteHost.Get(0), 0, 255, 0); // green

    // Run simulation
    Simulator::Stop(Seconds(10.5));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}