#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main (int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

    // Create nodes: 2 UEs, 1 eNodeB, 1 remote host
    NodeContainer ueNodes;
    ueNodes.Create (2);
    NodeContainer enbNode;
    enbNode.Create (1);
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create (1);
    Ptr<Node> remoteHost = remoteHostContainer.Get (0);

    // Install Internet stack on remote host
    InternetStackHelper internet;
    internet.Install (remoteHostContainer);

    // Create EPC helper
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
    lteHelper->SetEpcHelper (epcHelper);

    // Create PGW node
    Ptr<Node> pgw = epcHelper->GetPgwNode ();

    // Create P2P connection between PGW and remote host
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gbps")));
    p2ph.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (1)));
    NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);

    // Assign IP addresses to the p2p link
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
    // interface 0 is pgw, 1 is remoteHost
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);

    // Set default route for remoteHost
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
    remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

    // Assign mobility to eNodeB and UEs
    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (enbNode);
    mobility.Install (ueNodes);
    // Set positions
    enbNode.Get (0)->GetObject<MobilityModel> ()->SetPosition (Vector (0.0, 0.0, 0.0));
    ueNodes.Get (0)->GetObject<MobilityModel> ()->SetPosition (Vector (30.0, 0.0, 0.0));
    ueNodes.Get (1)->GetObject<MobilityModel> ()->SetPosition (Vector (60.0, 0.0, 0.0));

    // Install LTE devices to eNodeB and UEs
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNode);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

    // Install the IP stack on the UEs
    internet.Install (ueNodes);

    // Assign IP address to UEs, and attach to eNodeB
    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));
    for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
        lteHelper->Attach (ueLteDevs.Get (u), enbLteDevs.Get (0));
        // Set default route for UEs to the PGW
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (
                    ueNodes.Get(u)->GetObject<Ipv4> ());
        ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

    // Install UDP server on remote host (ports 8000, 8001 for each UE)
    uint16_t ports[2] = {8000, 8001};
    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < 2; ++i)
    {
        UdpServerHelper server (ports[i]);
        ApplicationContainer serverApp = server.Install (remoteHost);
        serverApp.Start (Seconds (1.0));
        serverApp.Stop (Seconds (10.0));
        serverApps.Add (serverApp);
    }

    // Install UDP clients on UEs
    for (uint32_t i = 0; i < 2; ++i)
    {
        UdpClientHelper client (remoteHostAddr, ports[i]);
        client.SetAttribute ("MaxPackets", UintegerValue (320));
        client.SetAttribute ("Interval", TimeValue (MilliSeconds (50)));
        client.SetAttribute ("PacketSize", UintegerValue (1024));
        ApplicationContainer clientApp = client.Install (ueNodes.Get (i));
        clientApp.Start (Seconds (2.0));
        clientApp.Stop (Seconds (9.0));
    }

    // Enable tracing
    lteHelper->EnableTraces ();

    // NetAnim setup
    AnimationInterface anim ("lte-simple-netanim.xml");
    anim.SetConstantPosition (enbNode.Get (0), 0.0, 0.0);
    anim.SetConstantPosition (ueNodes.Get (0), 30.0, 0.0);
    anim.SetConstantPosition (ueNodes.Get (1), 60.0, 0.0);
    anim.SetConstantPosition (remoteHost, 100.0, 0.0);
    anim.SetConstantPosition (pgw, 80.0, 0.0);

    Simulator::Stop (Seconds (10.0));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}