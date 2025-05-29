#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time simTime = Seconds(10.0);

    // Create nodes
    NodeContainer ueNodes;
    ueNodes.Create(2);
    NodeContainer enbNode;
    enbNode.Create(1);
    NodeContainer pgwNode;
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    // Install Internet Stack on remote host
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // LTE Helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Create the internet to PGW link
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

    // Assign IP addresses to the internet
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

    // Allow remoteHost to forward packets
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(
        Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(enbNode);

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
        "MinX", DoubleValue(50.0),
        "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(5.0),
        "DeltaY", DoubleValue(10.0),
        "GridWidth", UintegerValue(2),
        "LayoutType", StringValue("RowFirst"));
    mobility.Install(ueNodes);

    // Install LTE Devices to the nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNode);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on the UEs
    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Attach all UEs to the eNodeB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        lteHelper->Attach(ueLteDevs.Get(i), enbLteDevs.Get(0));
    }

    // Add static routes for UEs
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<Node> ue = ueNodes.Get(i);
        // interface 0 is localhost, 1 is wwan
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ue->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // UDP server on remote host
    uint16_t dlPort = 1234;
    ApplicationContainer serverApps;
    UdpServerHelper server(dlPort);
    serverApps = server.Install(remoteHost);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(simTime);

    // UDP client on each UE
    ApplicationContainer clientApps;
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        UdpClientHelper client(remoteHostAddr, dlPort);
        client.SetAttribute("MaxPackets", UintegerValue(10000000));
        client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
        client.SetAttribute("PacketSize", UintegerValue(1024));
        clientApps.Add(client.Install(ueNodes.Get(u)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(simTime);

    // Enable NetAnim tracing
    AnimationInterface anim("lte-simple-netanim.xml");
    anim.SetConstantPosition(enbNode.Get(0), 0, 0);
    anim.SetConstantPosition(ueNodes.Get(0), 50, 0);
    anim.SetConstantPosition(ueNodes.Get(1), 55, 0);
    anim.SetConstantPosition(remoteHost, 100, 0);

    // Enable pcap tracing on the p2p link (PGW <-> Internet)
    p2ph.EnablePcapAll("lte-simple");

    Simulator::Stop(simTime);
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}