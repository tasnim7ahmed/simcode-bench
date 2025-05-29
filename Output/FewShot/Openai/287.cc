#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mmwave-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create gNB (1) and UE (2) nodes
    NodeContainer gNbNodes;
    gNbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(2);

    // Install mobility: gNB stationary, UEs random in 50x50 area
    MobilityHelper mobility;
    // gNB: stationary at center
    Ptr<ListPositionAllocator> gnbPosition = CreateObject<ListPositionAllocator>();
    gnbPosition->Add(Vector(25.0, 25.0, 3.0));
    mobility.SetPositionAllocator(gnbPosition);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gNbNodes);

    // UEs: random walk in 50x50 area
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                 "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(0.0, 50.0, 0.0, 50.0)),
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                             "Distance", DoubleValue(10.0));
    mobility.Install(ueNodes);

    // Install NR stack using the mmWaveHelper
    Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper>();
    mmwaveHelper->SetAttribute("PathlossModel", StringValue("ns3::FriisPropagationLossModel"));
    NetDeviceContainer gNbDevs = mmwaveHelper->InstallGnbDevice(gNbNodes);
    NetDeviceContainer ueDevs = mmwaveHelper->InstallUeDevice(ueNodes);

    // Install IP stack
    InternetStackHelper internet;
    internet.Install(gNbNodes);
    internet.Install(ueNodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("7.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer ueIpIface = ipv4h.Assign(ueDevs);

    // Attach UEs to gNB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        mmwaveHelper->Attach(ueDevs.Get(i), gNbDevs.Get(0));
    }

    // Enable IP forwarding on the gNB
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> gNbStaticRouting = ipv4RoutingHelper.GetStaticRouting(gNbNodes.Get(0)->GetObject<Ipv4>());
    gNbStaticRouting->SetDefaultRoute("7.0.0.2", 1);

    // UDP server on UE 0, client on UE 1
    uint16_t port = 5001;

    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper udpClient(ueIpIface.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(10000));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable PCAP tracing
    mmwaveHelper->EnableTraces();
    mmwaveHelper->EnablePcapAll("mmwave-nr-one-gnb-two-ues");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}