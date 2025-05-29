#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteTcpExample");

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create LTE Helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

    // Create EPC Helper
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Create Remote Host
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // Create Internet Stack Helper
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("0ms"));
    NetDeviceContainer internetDevices = pointToPoint.Install(remoteHost, epcHelper->GetPgwNode());
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(0);

    // Routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create eNodeB
    NodeContainer enbContainer;
    enbContainer.Create(1);
    lteHelper->SetEnbDeviceAttribute("DlEarfcn", UintegerValue(100));
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbContainer);

    // Create UE
    NodeContainer ueContainer;
    ueContainer.Create(2);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueContainer);

    // Attach UE to eNodeB
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));
    lteHelper->Attach(ueLteDevs.Get(1), enbLteDevs.Get(0));

    // Set UE address
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Set default gateway for UE
    for (uint32_t u = 0; u < ueContainer.GetN(); ++u) {
        Ptr<Node> ueNode = ueContainer.Get(u);
        Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetStaticRouting(ueNode->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Create Applications
    uint16_t dlPort = 8080;

    // Install Server (UE 0)
    PacketSinkHelper dlPacketSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), dlPort));
    ApplicationContainer dlPacketSinkApp = dlPacketSinkHelper.Install(ueContainer.Get(0));
    dlPacketSinkApp.Start(Seconds(1.0));
    dlPacketSinkApp.Stop(Seconds(10.0));

    // Install Client (UE 1)
    Address dlServerAddress(InetSocketAddress(ueIpIface.GetAddress(0), dlPort));
    BulkSendHelper dlBulkSendHelper("ns3::TcpSocketFactory", dlServerAddress);
    dlBulkSendHelper.SetAttribute("MaxBytes", UintegerValue(1000000));
    ApplicationContainer dlBulkSendApp = dlBulkSendHelper.Install(ueContainer.Get(1));
    dlBulkSendApp.Start(Seconds(2.0));
    dlBulkSendApp.Stop(Seconds(10.0));

    // Run Simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}