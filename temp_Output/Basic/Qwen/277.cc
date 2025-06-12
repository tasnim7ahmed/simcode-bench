#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/config-store-module.h"
#include "ns3/nr-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set simulation time and packet interval
    double simTime = 10.0;
    double interPacketInterval = 1.0;

    // Create nodes: one gNodeB and one UE
    NodeContainer gnbNodes;
    NodeContainer ueNodes;
    gnbNodes.Create(1);
    ueNodes.Create(1);

    // Setup mobility for the UE with Random Walk in 100x100 area
    MobilityHelper mobilityUe;
    mobilityUe.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)),
                                "Distance", DoubleValue(5.0));
    mobilityUe.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                    "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                    "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobilityUe.Install(ueNodes);

    // Mobility for static gNodeB
    MobilityHelper mobilityGnb;
    mobilityGnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityGnb.Install(gnbNodes);

    // Create and configure the NR spectrum
    Ptr<NrSpectrumPhy> gnbSpectrumPhy = CreateObject<NrSpectrumPhy>();
    Ptr<NrSpectrumPhy> ueSpectrumPhy = CreateObject<NrSpectrumPhy>();

    // Install NR devices
    NrPointToPointEpcHelper epcHelper;
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();

    NetDeviceContainer gnbDevs = nrHelper->InstallGnbDevice(gnbNodes, epcHelper.GetSpectrumPhy());
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice(ueNodes, epcHelper.GetSpectrumPhy());

    // Attach UEs to gNBs
    nrHelper->AttachToClosestGnb(ueDevs, gnbDevs);

    // Install internet stack
    InternetStackHelper internet;
    internet.Install(gnbNodes);
    internet.Install(ueNodes);

    // Assign IP addresses
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper.AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Set default gateway for UEs
    Ipv4Address pgwAddr = epcHelper.GetPgwNode()->GetObject<Ipv4>()->GetInterface(0)->GetLocal(0);
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting<UdpServer>(ueNodes.Get(i)->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(pgwAddr, 1);
    }

    // Set up TCP server on gNodeB
    uint16_t tcpPort = 8000;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(gnbNodes.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simTime));

    // Set up TCP client on UE
    AddressValue remoteAddress(InetSocketAddress(ueIpIface.GetAddress(0), tcpPort));
    Config::Set("/NodeList/1/ApplicationList/0/Remote", remoteAddress);

    OnOffHelper clientHelper("ns3::TcpSocketFactory", Address());
    clientHelper.SetAttribute("Remote", remoteAddress);
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("1000bps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(512));
    clientHelper.SetAttribute("MaxBytes", UintegerValue(5 * 512));

    ApplicationContainer clientApp = clientHelper.Install(ueNodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simTime));

    // Enable logging
    LogComponentEnable("NrUeMac", LOG_LEVEL_INFO);
    LogComponentEnable("NrGnbMac", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}