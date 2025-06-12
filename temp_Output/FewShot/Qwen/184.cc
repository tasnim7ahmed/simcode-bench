#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create LTE helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Create eNodeB and UE nodes
    NodeContainer enbNodes;
    NodeContainer ueNodes;
    enbNodes.Create(1);
    ueNodes.Create(2);

    // Install LTE devices
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install internet stack on UEs and PGW node
    InternetStackHelper internet;
    internet.Install(ueNodes);
    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Create remote host
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    internet.Install(remoteHostContainer);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    // Connect remote host to PGW via point-to-point link
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(1500));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));

    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetInterfaces = ipv4h.Assign(internetDevices);

    // Assign IP addresses to UEs
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UEs to eNodeB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        lteHelper->Attach(ueDevs.Get(i), enbDevs.Get(0));
    }

    // Set default gateway for remote host
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.255.255.0"), 1);

    // Install UDP echo server on the remote host
    uint16_t dlPort = 1234;
    UdpEchoServerHelper echoServer(dlPort);
    ApplicationContainer serverApps = echoServer.Install(remoteHostContainer.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Install UDP applications on UEs
    UdpEchoClientHelper echoClientHelper(interfaces::dual_stack_address_type(), dlPort);
    echoClientHelper.SetAttribute("MaxPackets", UintegerValue(5));
    echoClientHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClientHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u) {
        clientApps.Add(echoClientHelper.Install(ueNodes.Get(u)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Mobility configuration
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u) {
        mobility.Install(ueNodes.Get(u));
        ueNodes.Get(u)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(1.0, 0.0, 0.0));
    }

    // Animation output
    AnimationInterface anim("lte-udp-animation.xml");
    anim.SetMobilityPollInterval(Seconds(0.1));

    // Run simulation
    Simulator::Stop(Seconds(10));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}