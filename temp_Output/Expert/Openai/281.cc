#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set logging (optional)
    //LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    //LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Parameters
    uint16_t numUes = 3;
    uint16_t numEnb = 1;
    double simTime = 10.0;
    uint16_t dlPort = 9;
    uint32_t packetCount = 10;
    double packetInterval = 1.0;
    double areaSize = 50.0;

    // Create nodes
    NodeContainer ueNodes, enbNodes;
    enbNodes.Create(numEnb);
    ueNodes.Create(numUes);

    // Install Mobility
    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.Install(enbNodes);
    Ptr<MobilityModel> enbMm = enbNodes.Get(0)->GetObject<MobilityModel>();
    enbMm->SetPosition(Vector(areaSize/2, areaSize/2, 0));

    MobilityHelper ueMobility;
    ueMobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                   "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                   "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0.0, areaSize, 0.0, areaSize)),
                                "Distance", DoubleValue(5.0));
    ueMobility.Install(ueNodes);

    // Install LTE devices
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP addresses to UEs
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Create remote host (to serve as EPC Internet)
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internetHost;
    internetHost.Install(remoteHost);

    // Connect PGW and remoteHost
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gbps")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

    // Assign IP addresses to UEs
    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Attach UEs to eNodeB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        lteHelper->Attach(ueLteDevs.Get(i), enbLteDevs.Get(0));
    }

    // Set up routing
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Install UDP server on eNodeB (via remoteHost; UEs send to eNodeB's S1 interface IP)
    UdpServerHelper udpServer(dlPort);
    ApplicationContainer serverApps = udpServer.Install(remoteHost);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    // Install UDP clients on UEs
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        UdpClientHelper udpClient(remoteHostAddr, dlPort);
        udpClient.SetAttribute("MaxPackets", UintegerValue(packetCount));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(i));
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(simTime));
    }

    lteHelper->EnableTraces();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}