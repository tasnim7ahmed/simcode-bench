#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Create nodes: one eNB and one UE
    NodeContainer enbNodes;
    NodeContainer ueNodes;
    enbNodes.Create(1);
    ueNodes.Create(1);

    // Install mobility
    MobilityHelper mobility;

    // eNB: static at (50,50,0)
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    Ptr<MobilityModel> mob = enbNodes.Get(0)->GetObject<MobilityModel>();
    mob->SetPosition(Vector(50.0, 50.0, 0.0));

    // UE: Random walk within 100x100
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0.0, 100.0, 0.0, 100.0)),
                              "Distance", DoubleValue(10.0));
    mobility.Install(ueNodes);

    // Install LTE devices
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install Internet stack (must use EPC to enable IP)
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    // Assign IP addresses and connect UE to eNB
    Ipv4InterfaceContainer ueIpIfaces;
    Ptr<Node> pgw = epcHelper->GetPgwNode();
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("7.0.0.0", "255.0.0.0");
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // Route from UE to eNB
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNodes.Get(0)->GetObject<Ipv4>());
    ueStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Get IP addresses
    Ipv4Address ueAddr = ueIpIfaces.GetAddress(0);
    // eNB does not get an EPC IP by default; assign directly for local comms
    Ptr<Ipv4> enbIpv4 = enbNodes.Get(0)->GetObject<Ipv4>();
    enbIpv4->AddAddress(1, Ipv4InterfaceAddress(Ipv4Address("1.1.1.1"), Ipv4Mask("255.255.255.0")));
    enbIpv4->SetUp(1);

    // UDP server (sink) on eNB at port 9999
    uint16_t port = 9999;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(enbNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // UDP client on UE
    UdpClientHelper client(Ipv4Address("1.1.1.1"), port);
    client.SetAttribute("MaxPackets", UintegerValue(5));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = client.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}