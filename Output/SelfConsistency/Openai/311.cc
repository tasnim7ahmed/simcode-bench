#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set up logging if needed
    // LogComponentEnable ("BulkSendApplication", LOG_LEVEL_INFO);
    // LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);

    // Create nodes: 1 eNodeB, 2 UEs, 1 remote host (for EPC)
    NodeContainer ueNodes;
    ueNodes.Create(2);
    NodeContainer enbNode;
    enbNode.Create(1);
    NodeContainer remoteHost;
    remoteHost.Create(1);

    // Setup the LTE/EPC helpers
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
    lteHelper->SetEpcHelper (epcHelper);

    // Install basic Internet stack on remote host
    InternetStackHelper internet;
    internet.Install(remoteHost);

    // Create the internet backbone between remoteHost and EPC gateway
    Ptr<Node> pgw = epcHelper->GetPgwNode();

    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gbps")));
    p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.001)));

    NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost.Get(0));

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);

    // Needed for remotehost access to UEs
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);

    // Assign UEs IP addresses and install stack
    internet.Install(ueNodes);

    // Install Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install(ueNodes);
    mobility.Install(enbNode);

    // Set up LTE devices
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNode);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Attach UEs to eNB
    for(uint32_t i=0; i < ueNodes.GetN(); ++i)
    {
        lteHelper->Attach(ueLteDevs.Get(i), enbLteDevs.Get(0));
    }

    // Assign IP address to UEs
    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));

    // Set the default gateway for the UEs to be the EPC gateway
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (ueNodes.Get(u)->GetObject<Ipv4> ()->GetRoutingProtocol());
        ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

    // SERVER: UE 0 acts as a TCP server at port 8080
    uint16_t port = 8080;
    Address sinkAddress (InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer serverApps = packetSinkHelper.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // CLIENT: UE 1 acts as TCP client, sending 1,000,000 bytes to UE 0 at port 8080
    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", 
        InetSocketAddress(ueIpIfaces.GetAddress(0), port));
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(1000000));
    ApplicationContainer clientApps = bulkSendHelper.Install(ueNodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable packet forwarding between EPC <-> internet
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}