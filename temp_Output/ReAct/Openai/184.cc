#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes: eNodeB, 2 UEs, remote host
    Ptr<Node> remoteHost = CreateObject<Node>();
    NodeContainer ueNodes, enbNodes;
    ueNodes.Create(2);
    enbNodes.Create(1);

    // Install Internet stack on remote host
    InternetStackHelper internet;
    internet.Install(remoteHost);

    // Create point-to-point link between remote host and PGW
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gbps")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));

    // LTE components
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Connect remoteHost to PGW
    NodeContainer p2pNodes;
    p2pNodes.Add(remoteHost);
    p2pNodes.Add(pgw);
    NetDeviceContainer p2pDevices = p2ph.Install(p2pNodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(p2pDevices);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(0);

    // Routing on remote host
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Install LTE Devices
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Mobility (ENB fixed, UEs placed near)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(50.0),
                                 "MinY", DoubleValue(50.0),
                                 "DeltaX", DoubleValue(10.0),
                                 "DeltaY", DoubleValue(10.0),
                                 "GridWidth", UintegerValue(1),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.Install(ueNodes);

    // Attach UEs to eNodeB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        lteHelper->Attach(ueLteDevs.Get(i), enbLteDevs.Get(0));
    }

    // Install IP stack on UEs, assign IP address
    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Set default gateway for UEs
    for (uint32_t j = 0; j < ueNodes.GetN(); ++j)
    {
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(
            ueNodes.Get(j)->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // UDP traffic: 2 UEs send UDP to remote host
    uint16_t dlPort = 1234;
    ApplicationContainer serverApps;
    UdpServerHelper myServer(dlPort);
    // Install server on remote host
    serverApps.Add(myServer.Install(remoteHost));
    serverApps.Start(Seconds(0.1));
    serverApps.Stop(Seconds(10.0));

    // Install UDP client on each UE
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        UdpClientHelper client(remoteHostAddr, dlPort);
        client.SetAttribute("MaxPackets", UintegerValue(320));
        client.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
        client.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApps = client.Install(ueNodes.Get(i));
        clientApps.Start(Seconds(0.2 + 0.1*i));
        clientApps.Stop(Seconds(10.0));
    }

    // Enable NetAnim
    AnimationInterface anim("lte-simple.xml");
    anim.SetConstantPosition(enbNodes.Get(0), 50.0, 50.0);
    anim.SetConstantPosition(ueNodes.Get(0), 60.0, 50.0);
    anim.SetConstantPosition(ueNodes.Get(1), 60.0, 60.0);
    anim.SetConstantPosition(remoteHost, 10.0, 50.0);

    // Enable packet capture
    p2ph.EnablePcapAll("lte-pgw-remotehost");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}