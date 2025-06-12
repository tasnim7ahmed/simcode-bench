#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteUdpMobilitySimulation");

int main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(1);

    MobilityHelper mobilityEnb;
    mobilityEnb.SetPositionAllocator("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue(0.0),
                                     "MinY", DoubleValue(0.0),
                                     "DeltaX", DoubleValue(0.0),
                                     "DeltaY", DoubleValue(0.0),
                                     "GridWidth", UintegerValue(1),
                                     "LayoutType", StringValue("RowFirst"));
    mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityEnb.Install(enbNodes);

    MobilityHelper mobilityUe;
    mobilityUe.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                    "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                    "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobilityUe.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Mode", StringValue("Time"),
                                "Time", TimeValue(Seconds(1.0)),
                                "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                                "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobilityUe.Install(ueNodes);

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(10.0, 0.0, 0.0));

    // LTE setup
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetAttribute("UseCa", BooleanValue(false));
    lteHelper->SetAttribute("FadingModel", StringValue("ns3::TraceFadingLossModel"));

    // Install LTE devices
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("11.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = ipv4h.Assign(ueLteDevs);
    Ipv4InterfaceContainer enbIpIface;
    enbIpIface = ipv4h.Assign(enbLteDevs);

    // Attach UEs to eNB
    lteHelper->Attach(ueLteDevs, enbLteDevs.Get(0));

    uint16_t dlPort = 1234;
    uint16_t ulPort = 2000;
    uint16_t otherPort = 3000;

    // Downlink: Server on eNB, client on UE sending packets to server
    UdpEchoServerHelper echoServer(dlPort);
    ApplicationContainer serverApps = echoServer.Install(enbNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(ueIpIface.GetAddress(0), dlPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = echoClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Uplink: Server on UE, client on eNB sending packets to server
    UdpEchoServerHelper echoServerUl(ulPort);
    ApplicationContainer serverAppsUl = echoServerUl.Install(ueNodes.Get(0));
    serverAppsUl.Start(Seconds(0.0));
    serverAppsUl.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClientUl(enbIpIface.GetAddress(0), ulPort);
    echoClientUl.SetAttribute("MaxPackets", UintegerValue(10));
    echoClientUl.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClientUl.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientAppsUl = echoClientUl.Install(enbNodes.Get(0));
    clientAppsUl.Start(Seconds(2.0));
    clientAppsUl.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}