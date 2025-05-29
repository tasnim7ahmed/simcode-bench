#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer ueNodes;
    ueNodes.Create(1);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Install Mobility
    // eNodeB: fixed; UE: RandomWalk2dMobilityModel
    MobilityHelper enbMob;
    enbMob.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMob.Install(enbNodes);

    MobilityHelper ueMob;
    ueMob.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                          "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)),
                          "Distance", DoubleValue(10.0),
                          "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"));
    ueMob.Install(ueNodes);

    // LTE Setup
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetAttribute("UseS1UForUdp", BooleanValue(true));

    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ueIpIface = ipv4h.Assign(ueLteDevs);
    Ipv4InterfaceContainer enbIpIface = ipv4h.Assign(enbLteDevs);

    // Attach UE to eNodeB
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    // Enable routing
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNodes.Get(0)->GetObject<Ipv4>());
    ueStaticRouting->SetDefaultRoute(enbIpIface.GetAddress(0), 1);

    Ptr<Ipv4StaticRouting> enbStaticRouting = ipv4RoutingHelper.GetStaticRouting(enbNodes.Get(0)->GetObject<Ipv4>());
    enbStaticRouting->SetDefaultRoute(ueIpIface.GetAddress(0), 1);

    // UDP Echo server on UE
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Echo client on eNodeB (target: UE)
    UdpEchoClientHelper echoClient(ueIpIface.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = echoClient.Install(enbNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}