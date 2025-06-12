#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/config-store-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

    CommandLine cmd;
    cmd.Parse(argc, argv);

    Config::SetDefault ("ns3::LteEnbNetDevice::DlEarfcn", UintegerValue (100));
    Config::SetDefault ("ns3::LteEnbNetDevice::UlEarfcn", UintegerValue (18100));

    NodeContainer enbNodes;
    enbNodes.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(2);

    // Create EPC helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Create a remote host
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // Create the internet link between the remote host and the EPC
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2ph.SetDeviceAttribute("Delay", StringValue("0ms"));
    NetDeviceContainer internetDevices = p2ph.Install(remoteHostContainer, epcHelper->GetPgwNode());
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(0);

    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(epcHelper->GetPgwNode()->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), Ipv4Mask("255.255.255.0"), 1);

    // Install LTE Devices to the nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Attach a UE to a eNodeB
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));
    lteHelper->Attach(ueLteDevs.Get(1), enbLteDevs.Get(0));

    // Set the default gateway for the UE
    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        Ptr<Node> ueNode = ueNodes.Get(u);
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNode->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Create Applications
    uint16_t dlPort = 10000;
    uint16_t ulPort = 20000;

    // Install and start applications on UEs and remote host

    UdpClientHelper clientHelper (remoteHostAddr, dlPort);
    clientHelper.SetAttribute ("MaxPackets", UintegerValue (1000));
    clientHelper.SetAttribute ("Interval", TimeValue (MilliSeconds (1)));
    clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
    ApplicationContainer clientApps = clientHelper.Install (ueNodes.Get (0));
    clientApps.Start (Seconds (1.0));
    clientApps.Stop (Seconds (10.0));


    UdpServerHelper serverHelper (dlPort);
    ApplicationContainer serverApps = serverHelper.Install (remoteHost);
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (10.0));

    UdpClientHelper clientHelper2 (ueIpIface.GetAddress (0), ulPort);
    clientHelper2.SetAttribute ("MaxPackets", UintegerValue (1000));
    clientHelper2.SetAttribute ("Interval", TimeValue (MilliSeconds (1)));
    clientHelper2.SetAttribute ("PacketSize", UintegerValue (1024));
    ApplicationContainer clientApps2 = clientHelper2.Install (remoteHost);
    clientApps2.Start (Seconds (1.0));
    clientApps2.Stop (Seconds (10.0));

    UdpServerHelper serverHelper2 (ulPort);
    ApplicationContainer serverApps2 = serverHelper2.Install (ueNodes.Get (0));
    serverApps2.Start (Seconds (1.0));
    serverApps2.Stop (Seconds (10.0));

    // Enable pcap tracing
    //p2ph.EnablePcapAll("lte-epc");

    // Animation Interface
    AnimationInterface anim ("lte-epc.xml");
    anim.SetConstantPosition(enbNodes.Get(0), 10.0, 10.0);
    anim.SetConstantPosition(ueNodes.Get(0), 20.0, 20.0);
    anim.SetConstantPosition(ueNodes.Get(1), 30.0, 30.0);
    anim.SetConstantPosition(remoteHost, 40.0, 40.0);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}