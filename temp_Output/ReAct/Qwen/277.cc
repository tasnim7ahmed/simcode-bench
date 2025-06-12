#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/nr-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NrTcpSimulation");

int main(int argc, char *argv[])
{
    uint16_t gNbNum = 1;
    uint16_t ueNum = 1;
    double simTime = 10.0;
    double packetSize = 512;
    double interval = 1.0;

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));

    NodeContainer gnbNodes;
    gnbNodes.Create(gNbNum);

    NodeContainer ueNodes;
    ueNodes.Create(ueNum);

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));

    MobilityHelper mobilityGnb;
    mobilityGnb.SetPositionAllocator(positionAlloc);
    mobilityGnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityGnb.Install(gnbNodes);

    MobilityHelper mobilityUe;
    mobilityUe.SetPositionAllocator<RandomDiscPositionAllocator>(
        "X", StringValue("100.0"),
        "Y", StringValue("100.0"),
        "Z", DoubleValue(0.0));
    mobilityUe.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)),
                                "Distance", DoubleValue(1.0));
    mobilityUe.Install(ueNodes);

    nr::NrHelper nrHelper;

    nrHelper.SetSchedulerTypeId(nr::NrMacSchedulerTdmaRR::GetTypeId());

    NetDeviceContainer gnbDevs;
    NetDeviceContainer ueDevs;

    nrHelper.SetGnbDeviceAttribute("Numerology", UintegerValue(1));
    nrHelper.SetUeDeviceAttribute("Numerology", UintegerValue(1));

    gnbDevs = nrHelper.InstallGnbDevice(gnbNodes, ueNodes);
    ueDevs = nrHelper.InstallUeDevice(ueNodes, gnbNodes);

    InternetStackHelper internet;
    internet.Install(gnbNodes);
    internet.Install(ueNodes);

    Ipv4AddressHelper ipv4GnbIpIfs;
    Ipv4AddressHelper ipv4UeIpIfs;
    ipv4GnbIpIfs.SetBase("1.0.0.0", "255.0.0.0");
    ipv4UeIpIfs.SetBase("2.0.0.0", "255.0.0.0");

    Ipv4InterfaceContainer gnbIpIfs = ipv4GnbIpIfs.Assign(gnbDevs);
    Ipv4InterfaceContainer ueIpIfs = ipv4UeIpIfs.Assign(ueDevs);

    uint16_t dlPort = 12345;

    PacketSinkHelper dlPacketSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), dlPort));
    ApplicationContainer dlApps = dlPacketSinkHelper.Install(gnbNodes.Get(0));
    dlApps.Start(Seconds(0.0));
    dlApps.Stop(Seconds(simTime));

    OnOffHelper onoff("ns3::TcpSocketFactory", Address());
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("100000000bps")));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff.SetAttribute("MaxBytes", UintegerValue(packetSize * 5));

    AddressValue remoteAddress(InetSocketAddress(gnbIpIfs.GetAddress(0), dlPort));
    onoff.SetAttribute("Remote", remoteAddress);

    ApplicationContainer clientApps = onoff.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}