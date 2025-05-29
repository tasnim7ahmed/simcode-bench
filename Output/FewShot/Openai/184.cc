#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logs
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer ueNodes;
    ueNodes.Create(2);
    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    // Install Mobility model
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);
    mobility.Install(remoteHostContainer);

    // LTE Helper and EPC helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Install LTE Devices to the nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Create Internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(remoteHost);

    // Create remote host link
    Ptr<Node> pgw = epcHelper->GetPgwNode();
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gbps")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

    // Assign IP addresses
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

    // Set default route for remoteHost
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo("7.0.0.0", "255.0.0.0", 1);

    // Assign IPs to UEs and configure default routing
    Ipv4InterfaceContainer ueIpIfaces;
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        ueIpIfaces.Add(epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs.Get(u))));
    }

    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        Ptr<Node> ue = ueNodes.Get(u);
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ue->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Attach UEs to eNB
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        lteHelper->Attach(ueLteDevs.Get(u), enbLteDevs.Get(0));
    }

    // Install and start UDP server on remote host
    uint16_t dlPort = 9001;
    UdpServerHelper udpServer(dlPort);
    ApplicationContainer serverApps = udpServer.Install(remoteHost);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Install and start UDP client on each UE
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        UdpClientHelper udpClient(remoteHostAddr, dlPort);
        udpClient.SetAttribute("MaxPackets", UintegerValue(100));
        udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(200)));
        udpClient.SetAttribute("PacketSize", UintegerValue(512));
        ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(u));
        clientApps.Start(Seconds(2.0 + u));
        clientApps.Stop(Seconds(10.0));
    }

    // Set positions for NetAnim visualization
    Ptr<ConstantPositionMobilityModel> enbMob = enbNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
    enbMob->SetPosition(Vector(50.0, 50.0, 0.0));
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<ConstantPositionMobilityModel> ueMob = ueNodes.Get(i)->GetObject<ConstantPositionMobilityModel>();
        ueMob->SetPosition(Vector(20.0 + 30.0 * i, 10.0, 0.0));
    }
    Ptr<ConstantPositionMobilityModel> rhMob = remoteHost->GetObject<ConstantPositionMobilityModel>();
    rhMob->SetPosition(Vector(100.0, 50.0, 0.0));

    // Set up NetAnim
    AnimationInterface anim("lte-netanim.xml");
    anim.SetConstantPosition(enbNodes.Get(0), 50.0, 50.0);
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        anim.SetConstantPosition(ueNodes.Get(i), 20.0 + 30.0 * i, 10.0);
    }
    anim.SetConstantPosition(remoteHost, 100.0, 50.0);

    // Enable packet metadata
    lteHelper->EnableTraces();

    // Run simulation
    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}