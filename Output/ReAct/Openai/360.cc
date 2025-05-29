#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes: 1 eNB, 1 UE
    NodeContainer ueNodes;
    ueNodes.Create(1);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Install Mobility
    MobilityHelper mobility;
    // eNB static at (0, 0, 0)
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    Ptr<MobilityModel> enbMobility = enbNodes.Get(0)->GetObject<MobilityModel>();
    enbMobility->SetPosition(Vector(0.0, 0.0, 0.0));
    // UE at (100, 0, 0)
    mobility.Install(ueNodes);
    Ptr<MobilityModel> ueMobility = ueNodes.Get(0)->GetObject<MobilityModel>();
    ueMobility->SetPosition(Vector(100.0, 0.0, 0.0));

    // Install LTE devices to nodes
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install IP stack
    InternetStackHelper internet;
    internet.Install(enbNodes);
    internet.Install(ueNodes);

    // Assign IP addresses to UEs
    Ipv4InterfaceContainer ueIpIface;
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("10.0.0.0", "255.255.255.0");

    // Install PGW for end-to-end IP connectivity
    Ptr<Node> pgw = lteHelper->GetPgwNode();
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    // Set up internet stack on remote host
    InternetStackHelper internetRemote;
    internetRemote.Install(remoteHost);

    // Create a point-to-point link between PGW and remoteHost
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gbps")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

    // Assign IP address to remoteHost and PGW
    ipv4h.SetBase("1.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer internetIfaces = ipv4h.Assign(internetDevices);

    // Set up a static route from remoteHost to UE network via PGW
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
        ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo("10.0.0.0", "255.255.255.0", 1);

    // Assign IP address to UE node
    ueIpIface = lteHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));
    // Attach UE to eNB
    lteHelper->Attach(ueDevs.Get(0), enbDevs.Get(0));

    // Route: set default gateway for UE to PGW
    Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(
        ueNodes.Get(0)->GetObject<Ipv4>());
    ueStaticRouting->SetDefaultRoute(lteHelper->GetUeDefaultGatewayAddress(), 1);

    // UDP Server: on eNB node, listening on port 8000
    uint16_t serverPort = 8000;
    UdpServerHelper udpServer(serverPort);
    ApplicationContainer serverApps = udpServer.Install(enbNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Client: on UE node, sending to eNB IP on port 8000
    Ipv4Address enbAddr = enbNodes.Get(0)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal();
    if (enbAddr.IsEqual(Ipv4Address("0.0.0.0")))
    {
        // In LTE simulation, more reliable to get the interface from helper
        // But here, enbNodes.Get(0) is on LTE device, likely with no IP
        // Use PGW's IP as eNB (server) address for testing
        enbAddr = internetIfaces.GetAddress(0);
    }

    UdpClientHelper udpClient(ueIpIface.GetAddress(0), serverPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(0xffffffff));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    
    ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Configure reply from eNB: send a UDP client from eNB to UE whenever packet is received
    // For this, set up a UDP EchoServer on UE and EchoClient on eNB
    uint16_t replyPort = 9000;
    UdpServerHelper ueReplyServer(replyPort);
    ApplicationContainer ueReplyServerApp = ueReplyServer.Install(ueNodes.Get(0));
    ueReplyServerApp.Start(Seconds(1.0));
    ueReplyServerApp.Stop(Seconds(10.0));

    UdpClientHelper enbReplyClient(ueIpIface.GetAddress(0), replyPort);
    enbReplyClient.SetAttribute("MaxPackets", UintegerValue(0xffffffff));
    enbReplyClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    enbReplyClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer enbReplyApps = enbReplyClient.Install(enbNodes.Get(0));
    enbReplyApps.Start(Seconds(2.0));
    enbReplyApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}