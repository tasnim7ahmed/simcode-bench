#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-helper.h"
#include "ns3/epc-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Set default TCP variant to NewReno
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));

    // Create LTE and EPC helpers
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Create a remote host and connect it to the P-GW
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internetStackHelper;
    internetStackHelper.Install(remoteHost);

    // Connect the remote host to the P-GW
    PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2pHelper.SetChannelAttribute("Delay", StringValue("10ms"));
    NetDeviceContainer internetDevices = p2pHelper.Install(epcHelper->GetPgwNode(), remoteHost);
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.255.255.0");
    ipv4h.Assign(internetDevices); // Assign IP addresses to the P2P devices

    // Create eNodeB and UE nodes
    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(2);

    // Install mobility model on eNodeB and UEs
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));  // eNodeB at origin
    positionAlloc->Add(Vector(10.0, 0.0, 0.0)); // UE1
    positionAlloc->Add(Vector(0.0, 10.0, 0.0)); // UE2

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);
    mobility.Install(remoteHostContainer); // Also install mobility on remote host for NetAnim

    // Install LTE devices to eNodeB and UEs
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on the UEs
    internetStackHelper.Install(ueNodes);

    // Attach UEs to eNodeB
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));
    lteHelper->Attach(ueLteDevs.Get(1), enbLteDevs.Get(0));

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup TCP applications for UE-to-UE communication via EPC
    // UE1 sends to UE2
    uint16_t sinkPort = 9;

    // Get IP address of UE2 for the sender to connect to
    Ptr<Ipv4> ue2Ipv4 = ueNodes.Get(1)->GetObject<Ipv4>();
    Ipv4Address ue2IpAddr = ue2Ipv4->GetAddress(1, 0).GetLocal(); // Get address of first LTE interface

    // PacketSink on UE2
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory",
                                      InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(ueNodes.Get(1)); // UE2 is receiver
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    // OnOffApplication on UE1
    OnOffHelper onOffHelper("ns3::TcpSocketFactory",
                            InetSocketAddress(ue2IpAddr, sinkPort)); // UE1 sends to UE2's IP
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));
    onOffHelper.SetAttribute("DataRate", StringValue("1Mbps"));
    ApplicationContainer clientApps = onOffHelper.Install(ueNodes.Get(0)); // UE1 is sender
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(9.0));

    // NetAnim visualization
    AnimationInterface anim("lte-epc-tcp.xml");
    anim.SetConstantPosition(enbNodes.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(ueNodes.Get(0), 10.0, 0.0);
    anim.SetConstantPosition(ueNodes.Get(1), 0.0, 10.0);
    anim.SetConstantPosition(remoteHost, 20.0, 0.0);
    anim.SetConstantPosition(epcHelper->GetSgwNode(), -5.0, -5.0); // SGW visualization
    anim.SetConstantPosition(epcHelper->GetPgwNode(), -10.0, -10.0); // PGW visualization

    anim.EnablePacketMetadata(true);
    anim.EnableIpv4RouteTracking("lte-epc-tcp-routes.xml", Seconds(0.5), Seconds(9.5), Seconds(1.0));

    // Simulation execution
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}