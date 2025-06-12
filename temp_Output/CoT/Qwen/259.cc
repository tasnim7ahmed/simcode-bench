#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mmwave-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FiveGSimulation");

int main(int argc, char *argv[])
{
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer gnbNode;
    gnbNode.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(2);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gnbNode);

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)),
                              "Distance", DoubleValue(1.0));
    mobility.Install(ueNodes);

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // gNB at origin
    positionAlloc->Add(Vector(10.0, 0.0, 0.0)); // Initial UE positions
    positionAlloc->Add(Vector(-10.0, 0.0, 0.0));
    MobilityHelper mobilityPositions;
    mobilityPositions.SetPositionAllocator(positionAlloc);
    mobilityPositions.Install(gnbNode.Get(0));
    mobilityPositions.Install(ueNodes.Get(0));
    mobilityPositions.Install(ueNodes.Get(1));

    Config::SetDefault("ns3::MmWavePhyMacCommon::Numerology", UintegerValue(1));
    Config::SetDefault("ns3::MmWavePhyMacCommon::CenterFrequency", DoubleValue(28e9));
    Config::SetDefault("ns3::MmWavePhyMacCommon::Bandwidth", DoubleValue(100e6));
    Config::SetDefault("ns3::MmWaveEnbPhy::TxPower", DoubleValue(40.0));
    Config::SetDefault("ns3::MmWaveUePhy::TxPower", DoubleValue(23.0));

    mmwave::MmWaveHelper mmwaveHelper;
    mmwaveHelper.Initialize();

    NetDeviceContainer gnbDevs;
    NetDeviceContainer ueDevs;

    gnbDevs = mmwaveHelper.InstallGnbDevice(gnbNode, ueNodes);
    ueDevs = mmwaveHelper.InstallUeDevice(gnbNode, ueNodes);

    InternetStackHelper internet;
    internet.Install(gnbNode);
    internet.Install(ueNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer gnbInterfaces;
    Ipv4InterfaceContainer ueInterfaces;

    for (uint32_t i = 0; i < gnbDevs.GetN(); ++i)
    {
        Ipv4InterfaceContainer ifc = ipv4.Assign(gnbDevs.Get(i)->GetNetDevice(0));
        gnbInterfaces.Add(ifc);
        ipv4.NewNetwork();
    }

    for (uint32_t i = 0; i < ueDevs.GetN(); ++i)
    {
        Ipv4InterfaceContainer ifc = ipv4.Assign(ueDevs.Get(i)->GetNetDevice(0));
        ueInterfaces.Add(ifc);
        ipv4.NewNetwork();
    }

    mmwave::MmWaveRrcSapProvider* rrcSapProvider = mmwaveHelper.GetRrcSapProvider();
    mmwave::MmWaveRrcSapUser* rrcSapUser = mmwaveHelper.GetRrcSapUser();
    rrcSapProvider->SetCellId(1);
    rrcSapUser->RecvSystemInformation(gnbInterfaces.GetAddress(0), 1);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(ueNodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(ueInterfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}