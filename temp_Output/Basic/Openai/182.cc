#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create Nodes: 2 UEs, 1 eNodeB, 1 EPC Gateway
    NodeContainer ueNodes;
    ueNodes.Create(2);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Install Mobility Model
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    for (uint32_t i = 0; i < enbNodes.GetN(); ++i)
    {
        Ptr<ConstantPositionMobilityModel> enbPos = CreateObject<ConstantPositionMobilityModel>();
        enbNodes.Get(i)->AggregateObject(enbPos);
        enbPos->SetPosition(Vector(0.0, 0.0 + 40 * i, 0.0));
    }
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<ConstantPositionMobilityModel> uePos = CreateObject<ConstantPositionMobilityModel>();
        ueNodes.Get(i)->AggregateObject(uePos);
        uePos->SetPosition(Vector(10.0, 10.0 + 20 * i, 0.0));
    }

    // Create LTE/EPC helpers
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Install LTE Devices to the nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on the UEs
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP addresses to UEs
    Ipv4InterfaceContainer ueIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Attach all UEs to the eNodeB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        lteHelper->Attach(ueLteDevs.Get(i), enbLteDevs.Get(0));
    }

    // Set up default route for UEs to PGW
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Node> pgw = epcHelper->GetPgwNode();
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        Ptr<Node> ueNode = ueNodes.Get(u);
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNode->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Applications: TCP server on UE 1, client on UE 0 (UE 0 sends to UE 1)
    uint16_t sinkPort = 50000;
    Address sinkAddress(InetSocketAddress(ueIfaces.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(ueNodes.Get(1));
    sinkApps.Start(Seconds(0.2));
    sinkApps.Stop(Seconds(8.0));

    OnOffHelper clientHelper("ns3::TcpSocketFactory", sinkAddress);
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper.SetAttribute("StartTime", TimeValue(Seconds(0.5)));
    clientHelper.SetAttribute("StopTime", TimeValue(Seconds(8.0)));
    ApplicationContainer clientApps = clientHelper.Install(ueNodes.Get(0));

    // Enable NetAnim traces
    AnimationInterface anim("lte-epc-ue2ue.xml");
    anim.SetConstantPosition(enbNodes.Get(0), 0, 0);
    anim.SetConstantPosition(ueNodes.Get(0), 10, 10);
    anim.SetConstantPosition(ueNodes.Get(1), 10, 30);

    Simulator::Stop(Seconds(8.1));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}