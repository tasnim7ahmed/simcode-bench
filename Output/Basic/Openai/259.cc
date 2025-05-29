#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/mmwave-helper.h"
#include "ns3/mmwave-point-to-point-epc-helper.h"
#include "ns3/config-store.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes: one gNB, two UEs
    NodeContainer gnbNodes;
    gnbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(2);

    // Mobility for gNB: stationary
    MobilityHelper gnbMobility;
    gnbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    gnbMobility.Install(gnbNodes);
    gnbNodes.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(0.0, 0.0, 2.0));

    // Mobility for UEs: Random Walk
    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)),
                                "Distance", DoubleValue(10.0),
                                "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"));
    ueMobility.Install(ueNodes);

    // Create NR (mmWave) helper
    Ptr<mmwave::MmWaveHelper> mmwaveHelper = CreateObject<mmwave::MmWaveHelper>();
    mmwaveHelper->SetAttribute("UseEpc", BooleanValue(true));
    Ptr<mmwave::MmWavePointToPointEpcHelper> epcHelper = CreateObject<mmwave::MmWavePointToPointEpcHelper>();
    mmwaveHelper->SetEpcHelper(epcHelper);

    // Install NR Devices
    NetDeviceContainer gnbDevs = mmwaveHelper->InstallGnbDevice(gnbNodes);
    NetDeviceContainer ueDevs = mmwaveHelper->InstallUeDevice(ueNodes);

    // Attach all UEs to the gNB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        mmwaveHelper->Attach(ueDevs.Get(i), gnbDevs.Get(0));
    }

    // Install IP stack
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Get PGW and assign IP addresses
    Ptr<Node> pgw = epcHelper->GetPgwNode();

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");

    // Assign IP address to UEs
    Ipv4InterfaceContainer ueIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Set default route for UEs
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (ueNodes.Get(i)->GetObject<Ipv4>()->GetRoutingProtocol());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // UDP Echo server on UE 2 (index 1)
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(ueNodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Echo client on UE 1 (index 0), targets UE 2
    UdpEchoClientHelper echoClient(ueIfaces.GetAddress(1), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}