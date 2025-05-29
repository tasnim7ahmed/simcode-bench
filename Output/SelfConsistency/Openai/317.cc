#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes: one eNB and one UE
    NodeContainer ueNodes;
    NodeContainer enbNodes;
    ueNodes.Create(1);
    enbNodes.Create(1);

    // Install Mobility Model
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    // Create LTE devices
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on UEs
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP address to UE device
    Ipv4InterfaceContainer ueIpIfaces;
    Ptr<EpcHelper> epcHelper = lteHelper->GetEpcHelper();
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UEs to eNB
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // Set the default gateway for the UE
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNodes.Get(0)->GetObject<Ipv4>());
    ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);

    // Install the remote host behind PGW to simulate the eNB server
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    // Connect remote host to PGW
    Ptr<Node> pgw = epcHelper->GetPgwNode();
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.01)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

    // Install Internet stack on remote host
    internet.Install(remoteHostContainer);

    // Assign IP address to remote host
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer remoteHostIfaces = ipv4h.Assign(internetDevices);

    // Set routing on remote host
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = 
        ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Install UDP Server on remote host (simulating server at eNB side)
    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(remoteHost);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Install UDP Client on the UE
    UdpClientHelper udpClient(remoteHostIfaces.GetAddress(1), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable LTE output logging if desired
    // lteHelper->EnableTraces();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}