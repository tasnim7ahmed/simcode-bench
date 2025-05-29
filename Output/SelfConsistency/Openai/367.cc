#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Enable logging (optional)
    // LogComponentEnable("LteHelper", LOG_LEVEL_WARN);

    // Create nodes: one eNodeB and one UE
    NodeContainer ueNodes;
    ueNodes.Create(1);

    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Create remote host (to act as server behind the eNodeB)
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    // Set up the Internet stack for the remote host
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // Connect remote host and eNodeB with point-to-point link
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gbps")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));

    NetDeviceContainer internetDevices = p2ph.Install(enbNodes.Get(0), remoteHost);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);

    // Save the address of the remote host
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

    // LTE setup
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<EpcHelper> epcHelper = lteHelper->GetEpcHelper();

    // The PGW node (anchor between LTE and Internet)
    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Connect the PGW to the remote host using point-to-point link
    // (We need a separate link between PGW and remote host)
    NodeContainer pgwRemoteHost(pgw, remoteHost);
    NetDeviceContainer pgwToRemote = p2ph.Install(pgwRemoteHost);

    ipv4h.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer pgwIpIfaces = ipv4h.Assign(pgwToRemote);
    Ipv4Address ueDefaultGateway = pgwIpIfaces.GetAddress(0);

    // Add Internet stack to UE
    internet.Install(ueNodes);

    // Assign IP to UE via EPC
    NetDeviceContainer ueLteDevs;
    NetDeviceContainer enbLteDevs;

    // Install LTE devices to the nodes
    enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Assign IP address to UEs, and set up default routes
    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting>(ueNodes.Get(u)->GetObject<Ipv4>()->GetRoutingProtocol ());
        ueStaticRouting->SetDefaultRoute(ueDefaultGateway, 1);
    }

    // Attach UE to eNodeB
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    // Set up an echo server application on the remote host to receive TCP packets
    uint16_t port = 50000;
    Address serverAddress(InetSocketAddress(remoteHostAddr, port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApps = packetSinkHelper.Install(remoteHost);
    serverApps.Start(Seconds(0.1));
    serverApps.Stop(Seconds(10.0));

    // Set up a TCP client on the UE, sending data to the server
    OnOffHelper onoff("ns3::TcpSocketFactory", serverAddress);
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(0.5)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
    ApplicationContainer clientApps = onoff.Install(ueNodes.Get(0));

    // Start the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}