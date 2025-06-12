#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/nr-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NrTcpSimulation");

int main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Set up logging
    LogComponentEnable("NrTcpSimulation", LOG_LEVEL_INFO);

    // Create nodes: one gNodeB and one UE
    NodeContainer gnbNodes;
    gnbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Mobility for UE: Random Walk within 100x100 area
    MobilityHelper mobilityUe;
    mobilityUe.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                    "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                    "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobilityUe.SetMobilityModel("ns3::RandomWalk2DOutdoorMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobilityUe.Install(ueNodes);

    // Mobility for gNodeB: static
    MobilityHelper mobilityGnb;
    mobilityGnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityGnb.Install(gnbNodes);

    // Install NR stack
    NrPointToPointEpcHelper epcHelper;
    NrHelper nrHelper;

    // Spectrum configuration
    nrHelper.SetChannelType(NrHelper::UMi_StreetCanyon);
    nrHelper.SetSchedulerType("ns3::nr::bwp::FfMacSchedulerOfdmaRr");

    // Create the P2P backhaul link between GW and gNB
    PointToPointHelper backhaulLink;
    backhaulLink.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Gb/s")));
    backhaulLink.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));
    epcHelper.SetBackhaulLink(backhaulLink);

    NetDeviceContainer gnbDevs = nrHelper.InstallGnbDevice(gnbNodes, epcHelper.GetSpectrumModel());
    NetDeviceContainer ueDevs = nrHelper.InstallUeDevice(ueNodes, epcHelper.GetSpectrumModel());

    // Install EPC
    Ptr<Node> pgw = epcHelper.GetPgwNode();
    epcHelper.AddEnb(gnbNodes.Get(0), gnbDevs.Get(0));
    epcHelper.AttachUe(ueNodes.Get(0));

    // Install internet stack
    InternetStackHelper internet;
    internet.Install(gnbNodes);
    internet.Install(ueNodes);

    // Assign IP addresses
    Ipv4InterfaceContainer ueIpIface = epcHelper.AssignUeIpv4Address(NetDeviceContainer(ueDevs));
    Ipv4InterfaceContainer gnbIpIface = epcHelper.AssignGnbIpv4Address(NetDeviceContainer(gnbDevs));

    // TCP Server (gNodeB)
    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(ueIpIface.GetAddress(0), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(gnbNodes.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // TCP Client (UE)
    OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddress);
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(512));
    onoff.SetAttribute("MaxBytes", UintegerValue(5 * 512));
    ApplicationContainer clientApps = onoff.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // Enable logging from applications
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Simulation setup
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}