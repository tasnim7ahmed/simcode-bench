#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging for UdpClient/Server if desired
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    // Create a single eNodeB and UE
    NodeContainer ueNodes;
    ueNodes.Create(1);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Install Mobility: eNodeB stationary, UE random 
    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.Install(enbNodes);

    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)),
                                "Distance", DoubleValue(10.0),
                                "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"));
    ueMobility.Install(ueNodes);

    // Install LTE devices to the nodes
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on the UEs
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP addresses to UEs, via EPC
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Create a remote host for the backbone (not used for UE <-> eNB apps here)
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internetHost;
    internetHost.Install(remoteHost);

    // Set up the LTE IP interface
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UE to eNodeB
    lteHelper->Attach(ueDevs.Get(0), enbDevs.Get(0));

    // Now install UDP server on the UE
    uint16_t port = 5000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(ueNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Install UDP client on the eNodeB node
    InternetStackHelper inetNb;
    inetNb.Install(enbNodes);

    Ipv4Address ueIpAddr = ueIpIface.GetAddress(0);

    UdpClientHelper udpClient(ueIpAddr, port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000000));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = udpClient.Install(enbNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable routing on the eNodeB
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4> ipv4Enb = enbNodes.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> enbStaticRouting = ipv4RoutingHelper.GetStaticRouting(ipv4Enb);
    enbStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}