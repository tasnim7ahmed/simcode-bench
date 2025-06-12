#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::LteHelper::UseRlcSm", BooleanValue(true));
    Config::SetDefault("ns3::LteHelper::AnrEnabled", BooleanValue(false));
    Config::SetDefault("ns3::LteHelper::UsePdschForCqiGeneration", BooleanValue(true));

    NodeContainer enbNodes;
    enbNodes.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(2);

    Ptr<ListPositionAllocator> positionAllocEnb = CreateObject<ListPositionAllocator>();
    positionAllocEnb->Add(Vector(0.0, 0.0, 0.0));
    MobilityModelHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.Install(enbNodes);
    positionAllocEnb->Add(Vector(0.0, 0.0, 0.0));

    MobilityModelHelper ueMobility;
    ueMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    ueMobility.Install(ueNodes);

    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    PointToPointEpcHelper epcHelper;
    LteHelper lteHelper;
    lteHelper.SetEpcHelper(epcHelper.GetEpcHelper());

    lteHelper.SetPathlossModelType(TypeId::LookupByName("ns3::FriisSpectrumPropagationLossModel"));
    lteHelper.SetPathlossModelAttribute("Frequency", DoubleValue(2.4e9));
    lteHelper.SetComponentCarrierManagerType("ns3::NoOpComponentCarrierManager");

    NetDeviceContainer enbDevs;
    enbDevs = lteHelper.InstallEnbDevice(enbNodes.Get(0));

    NetDeviceContainer ueDevs;
    ueDevs.Add(lteHelper.InstallUeDevice(ueNodes.Get(0)));
    ueDevs.Add(lteHelper.InstallUeDevice(ueNodes.Get(1)));

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper.AssignUeIpv4Address(NetDeviceContainer(ueDevs.Get(0), ueDevs.Get(1)));

    lteHelper.Attach(ueDevs, enbDevs.Get(0));

    uint16_t dlPort = 8080;

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), dlPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(ueNodes.Get(0));
    sinkApps.Start(Seconds(1.0));
    sinkApps.Stop(Seconds(10.0));

    OnOffHelper clientHelper("ns3::TcpSocketFactory", Address());
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("1000Kbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

    AddressValue remoteAddress(InetSocketAddress(ueIpIfaces.GetAddress(0), dlPort));
    clientHelper.SetAttribute("Remote", remoteAddress);
    clientHelper.SetAttribute("MaxBytes", UintegerValue(1000000));

    ApplicationContainer clientApps = clientHelper.Install(ueNodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}