#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Create nodes: 1 eNB, 1 UE
    NodeContainer ueNodes;
    ueNodes.Create(1);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Install Mobility
    MobilityHelper mobility;

    // eNodeB at center and static
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    Ptr<MobilityModel> enbMobility = enbNodes.Get(0)->GetObject<MobilityModel>();
    enbMobility->SetPosition(Vector(25.0, 25.0, 0.0));

    // UE with RandomWalk2dMobility
    mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue (Rectangle (0, 50, 0, 50)),
                              "Distance", DoubleValue (5.0),
                              "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=2.0]"));
    mobility.Install(ueNodes);

    // Install LTE Devices
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install EPC
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
    lteHelper->SetEpcHelper(epcHelper);

    // PGW node
    Ptr<Node> pgw = epcHelper->GetPgwNode ();

    // Create remote host (simulating internet)
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    // Install Internet Stack
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // Connect PGW and remote host
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute ("DataRate", StringValue ("100Gbps"));
    p2ph.SetChannelAttribute ("Delay", StringValue ("1ms"));

    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIfaces.GetAddress(1);

    // Install IP stack on UEs
    internet.Install(ueNodes);

    // Assign IP address to UE devices and configure default route
    Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));
    lteHelper->Attach(ueDevs.Get(0), enbDevs.Get(0));
    epcHelper->AddRouteToUe(0, ueIpIface.GetAddress(0), ueDevs.Get(0));

    // Set default route for UE
    Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (ueNodes.Get(0)->GetObject<Ipv4> ()->GetRoutingProtocol ());
    ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);

    // Set up UDP server (to simulate HTTP server on eNodeB)
    uint16_t httpPort = 80;
    UdpServerHelper server(httpPort);
    ApplicationContainer serverApps = server.Install(ueNodes.Get(0)); // Should be on IP device, but as per LTE, eNB acts as IP router only, so server put on UE
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Set up UDP client on UE to send 5 packets at 1s interval
    UdpClientHelper client(ueIpIface.GetAddress(0), httpPort);
    client.SetAttribute("MaxPackets", UintegerValue(5));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = client.Install(remoteHost);
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // Enable pcap
    p2ph.EnablePcapAll("lte-simple");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}