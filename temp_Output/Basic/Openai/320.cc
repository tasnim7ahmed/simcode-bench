#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes: 1 eNodeB, 3 UEs
    NodeContainer ueNodes;
    ueNodes.Create(3);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Setup LTE devices
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

    // Setup EPC and remote host (to use IP on UE<->eNodeB)
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Create a remote host (for internet)
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    // Setup internet on remote host
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gbps")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));

    NetDeviceContainer internetDevices = p2ph.Install(epcHelper->GetPgwNode(), remoteHost);
    InternetStackHelper internet;
    internet.Install(remoteHost);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

    // Install Mobility Model
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    // Install LTE Devices to the nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on the UEs
    InternetStackHelper ueInternet;
    ueInternet.Install(ueNodes);

    // Assign IP address to UEs, and set up default gateway
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Attach each UE to the eNodeB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        lteHelper->Attach(ueLteDevs.Get(i), enbLteDevs.Get(0));
    }

    // Set up default gateway for UEs (to reach eNodeB/server via PGW)
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (ueNodes.Get(i)->GetObject<Ipv4>()->GetRoutingProtocol());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Install UDP server (on eNodeB)
    uint16_t serverPort = 9;
    UdpServerHelper udpServer(serverPort);
    ApplicationContainer serverApp = udpServer.Install(enbNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Each UE acts as UDP client, sending to the eNodeB
    // Get the IP address assigned to eNodeB's LTE device (use S1-U interface via PGW)
    // The address to send to: enb-to-pgw link is transparent to UEs, send to their gateway
    Ipv4Address enbAddr = ueIpIfaces.GetAddress(0); // as eNodeB is default route and will receive via S1

    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        UdpClientHelper udpClient(enbAddr, serverPort);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = udpClient.Install(ueNodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
    }

    lteHelper->EnableTraces();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}