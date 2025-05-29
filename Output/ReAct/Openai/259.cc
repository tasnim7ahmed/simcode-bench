#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/log.h"
#include "ns3/nr-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

    Time::SetResolution(Time::NS);

    // Create nodes: 1 gNB, 2 UEs
    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(2);

    // Install mobility
    MobilityHelper mobilityEnb;
    mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityEnb.Install(enbNodes);

    MobilityHelper mobilityUe;
    mobilityUe.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)),
                                "Distance", DoubleValue(5.0),
                                "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"));
    mobilityUe.Install(ueNodes);

    // Install NR devices
    // NR Helper
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();

    // Spectrum configuration for mmWave (e.g. 28 GHz)
    double centralFrequency = 28e9;
    double bandwidth = 100e6;

    // Helper: one sector, no X2, no EPC/NGC
    Ptr<ThreeGppChannelModel> channelModel = CreateObject<ThreeGppChannelModel>();
    nrHelper->SetChannelModelAttribute("ChannelModel", PointerValue(channelModel));

    // Setup Channel and PHY (Default Settings)
    nrHelper->SetAttribute("UsePdschForCqiGeneration", BooleanValue(true));
    nrHelper->SetGnbAntennaAttribute("NumRows", UintegerValue(2));
    nrHelper->SetGnbAntennaAttribute("NumColumns", UintegerValue(4));
    nrHelper->SetUeAntennaAttribute("NumRows", UintegerValue(2));
    nrHelper->SetUeAntennaAttribute("NumColumns", UintegerValue(2));

    BandwidthPartInfoPtrVector allBwps;
    CcBwpCreator ccBwpCreator;
    CcBwpCreatorResult bwps = ccBwpCreator.CreateCcBwpSingle(centralFrequency, bandwidth, BandwidthPartInfo::UMi_StreetCanyon);

    allBwps = bwps.m_Bwp;

    nrHelper->SetCcBwpCreator(ccBwpCreator);

    // Create NetDevices
    NetDeviceContainer enbNetDev = nrHelper->InstallGnbDevice(enbNodes, allBwps);
    NetDeviceContainer ueNetDev = nrHelper->InstallUeDevice(ueNodes, allBwps);

    // Attach UEs to GNB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        nrHelper->Attach(ueNetDev.Get(i), enbNetDev.Get(0));
    }

    // Install IP stack
    InternetStackHelper internet;
    internet.Install(enbNodes);
    internet.Install(ueNodes);

    // Assign IPs
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ueIpIfaces = ipv4h.Assign(ueNetDev);

    // Set routing for UEs (direct point-to-point like for this small topology)
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<Node> ueNode = ueNodes.Get(i);
        Ptr<Ipv4> ipv4 = ueNode->GetObject<Ipv4>();
        Ptr<Ipv4StaticRouting> staticRouting = ipv4RoutingHelper.GetStaticRouting(ipv4);
        staticRouting->SetDefaultRoute("192.168.1.1", 1); // Use first interface
    }

    // Create UDP echo server on UE2 (index 1), port 9
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(ueNodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Create UDP echo client on UE1 (index 0)
    UdpEchoClientHelper echoClient(ueIpIfaces.GetAddress(1), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Set simulation stop time
    Simulator::Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}