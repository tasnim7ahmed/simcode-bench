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

    // Create nodes: one eNodeB and one UE
    NodeContainer ueNodes;
    NodeContainer enbNodes;
    ueNodes.Create(1);
    enbNodes.Create(1);

    // Install Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    // Install LTE Devices to the nodes
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on UEs
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP address to UEs, get gateway address
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = lteHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Attach UE to eNodeB
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    // Set up remote host (to represent the eNodeB's EPC PGW interface)
    Ptr<Node> remoteHost;
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internetHost;
    internetHost.Install(remoteHostContainer);

    // Create a point-to-point link between PGW and remoteHost
    Ptr<Node> pgw = lteHelper->GetEpcHelper()->GetPgwNode();
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gbps")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.0001)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

    // Assign IP addresses for this link
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

    // Add static route on remote host to UE subnet via PGW
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Set up TCP server on remote host (to simulate eNodeB receiving TCP packets)
    uint16_t sinkPort = 50000;
    Address sinkAddress(InetSocketAddress(remoteHostAddr, sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(remoteHost);
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    // TCP client (BulkSend) on UE, sending to remote host
    BulkSendHelper clientHelper("ns3::TcpSocketFactory", sinkAddress);
    clientHelper.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer clientApps = clientHelper.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    lteHelper->EnableTraces();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}