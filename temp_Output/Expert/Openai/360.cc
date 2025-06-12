#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/lte-module.h"
#include "ns3/epc-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    Ptr<Node> pgw = epcHelper->GetPgwNode();

    NodeContainer ueNodes;
    NodeContainer enbNodes;
    enbNodes.Create(1);
    ueNodes.Create(1);

    // Install LTE Devices
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Internet Stack
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(pgw);

    // Assign IP addresses to UEs
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs), Ipv4Address("10.0.0.0"), Ipv4Mask("255.255.255.0"));

    // Attach UE to eNB
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    // Set up default routes
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNodes.Get(0)->GetObject<Ipv4>());
    ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);

    // UDP Server on eNB side (we use PGW for server, as eNB does not run IP stack)
    uint16_t serverPort = 50000;
    UdpServerHelper udpServer(serverPort);
    ApplicationContainer serverApps = udpServer.Install(pgw);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Client on UE
    UdpClientHelper udpClient(epcHelper->GetPgwNode()->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), serverPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // UDP Echo Server on UE (for response from eNB/PGW)
    uint16_t echoPort = 40000;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer echoServerApp = echoServer.Install(ueNodes.Get(0));
    echoServerApp.Start(Seconds(1.0));
    echoServerApp.Stop(Seconds(10.0));

    // UdpEchoClient on PGW to send response to UE
    UdpEchoClientHelper echoClient(ueIpIface.GetAddress(0), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer echoClientApp = echoClient.Install(pgw);
    echoClientApp.Start(Seconds(2.1));
    echoClientApp.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}