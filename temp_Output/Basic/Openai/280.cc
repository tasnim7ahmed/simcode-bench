#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create Nodes: 1 gNB and 2 UEs
    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(2);

    // Install Mobility
    MobilityHelper mobilityEnb;
    mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityEnb.Install(enbNodes);

    MobilityHelper mobilityUe;
    mobilityUe.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
    mobilityUe.Install(ueNodes);

    // Create NR helper
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();

    // Use the 3GPP channel model
    nrHelper->SetAttribute("ChannelConditionModel", StringValue("ns3::ThreeGppChannelConditionModel"));

    // Core network
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    nrHelper->SetEpcHelper(epcHelper);

    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Install NR devices to the nodes
    NetDeviceContainer enbDevs = nrHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice(ueNodes);

    // Attach UE devices to the gNB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        nrHelper->Attach(ueDevs.Get(i), enbDevs.Get(0));
    }

    // Internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(pgw);

    // Assign IP addresses to UEs, via EPC helper
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Set default gateway for UEs
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(ueNodes.Get(i)->GetObject<Ipv4>()->GetRoutingProtocol());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Install and bind TCP server on UE 1 (second UE)
    uint16_t port = 8080;
    Address serverAddr(InetSocketAddress(ueIpIfaces.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApps = packetSinkHelper.Install(ueNodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Install TCP client on UE 0 (first UE), sending to server on UE 1
    OnOffHelper clientHelper("ns3::TcpSocketFactory", serverAddr);
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper.SetAttribute("StartTime", TimeValue(Seconds(2.0)));
    clientHelper.SetAttribute("StopTime", TimeValue(Seconds(9.0)));
    ApplicationContainer clientApps = clientHelper.Install(ueNodes.Get(0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}