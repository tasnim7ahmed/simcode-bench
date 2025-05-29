#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging for TcpServer and TcpClient
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);

    // Create nodes: one eNodeB and two UEs
    NodeContainer ueNodes;
    ueNodes.Create(2);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Install Mobility Model
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(ueNodes);
    mobility.Install(enbNodes);

    // Create LTE devices and attach them
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install Internet stack on UEs
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP addresses to UEs
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = lteHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Attach UEs to eNodeB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        lteHelper->Attach(ueLteDevs.Get(i), enbLteDevs.Get(0));
    }

    // Install the EPC and get PGW
    Ptr<PointToPointEpcHelper> epcHelper = DynamicCast<PointToPointEpcHelper>(lteHelper->GetEpcHelper());
    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Setup a remote host; not strictly needed here as UEs talk to each other

    // Assign IP address helper for UEs (already done)

    // Set up TCP server on UE 0
    uint16_t port = 8080;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);

    ApplicationContainer serverApps = packetSinkHelper.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up TCP client (BulkSendApplication) on UE 1, send 1,000,000 bytes to UE 0
    BulkSendHelper bulkSender("ns3::TcpSocketFactory",
                              InetSocketAddress(ueIpIface.GetAddress(0), port));
    bulkSender.SetAttribute("MaxBytes", UintegerValue(1000000));
    bulkSender.SetAttribute("SendSize", UintegerValue(1024));

    ApplicationContainer clientApps = bulkSender.Install(ueNodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable routing on UEs
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNodes.Get(i)->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}