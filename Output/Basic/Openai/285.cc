#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes: eNodeB (1) + UE (3)
    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(3);

    // Install Mobility: eNodeB stationary, UEs mobile in 100x100 area
    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.Install(enbNodes);

    MobilityHelper ueMobility;
    ueMobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                   "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                   "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Mode", StringValue("Time"),
                                "Time", TimeValue(Seconds(0.5)),
                                "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                                "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    ueMobility.Install(ueNodes);

    // Install LTE Devices
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetAttribute("UseIdealRrc", BooleanValue(false));
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install Internet stack on UEs
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP addresses to UEs
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UEs to eNB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        lteHelper->Attach(ueDevs.Get(i), enbDevs.Get(0));
    }

    // Set default gateway for UEs
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<Node> ueNode = ueNodes.Get(i);
        Ptr<Ipv4> ipv4 = ueNode->GetObject<Ipv4>();
        Ptr<Ipv4StaticRouting> staticRouting = ipv4RoutingHelper.GetStaticRouting(ipv4);
        staticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Install UDP server on UE 0 (port 5000)
    uint16_t port = 5000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(0.1));
    serverApps.Stop(Seconds(10.0));

    // Install UDP client on UE 1 (send to UE 0)
    UdpClientHelper udpClient(ueIpIfaces.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(10000));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(1));
    clientApps.Start(Seconds(0.5));
    clientApps.Stop(Seconds(10.0));

    // Enable PCAP tracing
    lteHelper->EnablePcap("lte-enb", enbDevs.Get(0)->GetNode()->GetId(), 0, true, true);
    lteHelper->EnablePcap("lte-ue", ueDevs.Get(0)->GetNode()->GetId(), 0, true, true);
    lteHelper->EnablePcap("lte-ue", ueDevs.Get(1)->GetNode()->GetId(), 0, true, true);
    lteHelper->EnablePcap("lte-ue", ueDevs.Get(2)->GetNode()->GetId(), 0, true, true);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}