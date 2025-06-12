#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/mmwave-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FiveGSimulation");

int main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer gnbNodes;
    NodeContainer ueNodes;
    gnbNodes.Create(1);
    ueNodes.Create(2);

    Ptr<Node> gnbNode = gnbNodes.Get(0);
    Ptr<Node> ueServer = ueNodes.Get(0);
    Ptr<Node> ueClient = ueNodes.Get(1);

    MobilityHelper mobilityGnb;
    mobilityGnb.SetPositionAllocator("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue(0.0),
                                     "MinY", DoubleValue(0.0),
                                     "DeltaX", DoubleValue(0.0),
                                     "DeltaY", DoubleValue(0.0),
                                     "GridWidth", UintegerValue(1),
                                     "LayoutType", StringValue("RowFirst"));
    mobilityGnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityGnb.Install(gnbNode);

    MobilityHelper mobilityUe;
    mobilityUe.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                    "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                    "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    mobilityUe.SetMobilityModel("ns3::RandomWalk2DMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
    mobilityUe.Install(ueNodes);

    MmWaveHelper mmwave;
    mmwave.SetChannelConditionModel("ns3::MmWave3GPPChannelConditionModel");
    mmwave.SetPathlossModel("ns3::MmWave3gppPropagationLossModel");

    NetDeviceContainer gnbDevs;
    NetDeviceContainer ueDevs;

    mmwave.Install(gnbNodes, ueNodes, gnbDevs, ueDevs);

    InternetStackHelper internet;
    internet.Install(gnbNodes);
    internet.Install(ueNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer serverIfaces = ipv4.Assign(ueDevs.Get(0));
    Ipv4InterfaceContainer clientIfaces = ipv4.Assign(ueDevs.Get(1));

    UdpEchoServerHelper echoServer(5001);
    ApplicationContainer serverApps = echoServer.Install(ueServer);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(serverIfaces.GetAddress(0), 5001);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10000));
    echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(ueClient);
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    mmwave.EnableTraces();
    mmwave.EnablePhyTraces();
    mmwave.EnableMacTraces();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}