#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // LTE helpers
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Create eNodeB and UE nodes
    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(3);

    // Install Internet stack on UEs
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Set mobility for eNodeB (stationary)
    MobilityHelper enbMobility;
    Ptr<ListPositionAllocator> enbPosAlloc = CreateObject<ListPositionAllocator>();
    enbPosAlloc->Add(Vector(50.0, 50.0, 0.0));
    enbMobility.SetPositionAllocator(enbPosAlloc);
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.Install(enbNodes);

    // Set mobility for UEs (random mobility in 100x100 area)
    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0.0, 100.0, 0.0, 100.0)),
                                "Distance", DoubleValue(10.0));
    ueMobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                   "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                   "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    ueMobility.Install(ueNodes);

    // Install LTE Devices
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Attach UEs to eNodeB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        lteHelper->Attach(ueDevs.Get(i), enbDevs.Get(0));
    }

    // Assign IP addresses to UEs
    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Add static routing in UEs
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(ueNodes.Get(i)->GetObject<Ipv4>()->GetRoutingProtocol());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // UDP Server on UE 0, port 5000
    uint16_t serverPort = 5000;
    UdpServerHelper udpServer(serverPort);
    ApplicationContainer serverApp = udpServer.Install(ueNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP Client on UE 1
    UdpClientHelper udpClient(ueIpIfaces.GetAddress(0), serverPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(10000));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApp = udpClient.Install(ueNodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // PCAP Tracing
    lteHelper->EnablePcap("lte-enb", enbDevs.Get(0));
    lteHelper->EnablePcap("lte-ue0", ueDevs.Get(0));
    lteHelper->EnablePcap("lte-ue1", ueDevs.Get(1));
    lteHelper->EnablePcap("lte-ue2", ueDevs.Get(2));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}