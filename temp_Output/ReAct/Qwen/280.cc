#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/nr-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FiveGSimulation");

int main(int argc, char *argv[]) {
    uint16_t gNbNum = 1;
    uint16_t ueNum = 2;
    double simTime = 10.0;

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(8 * 1024 * 1024));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(8 * 1024 * 1024));

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    NodeContainer gNbs;
    gNbs.Create(gNbNum);

    NodeContainer uEs;
    uEs.Create(ueNum);

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // gNB at origin

    for (uint16_t i = 0; i < ueNum; ++i) {
        positionAlloc->Add(Vector(5.0 + i * 10.0, 5.0, 0.0));
    }

    MobilityHelper mobility;
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gNbs);

    MobilityHelper ueMobility;
    ueMobility.SetPositionAllocator<RandomBoxPositionAllocator>("X", StringValue("ns3::UniformRandomVariable[Min=0.0,Max=50.0]"),
                                                               "Y", StringValue("ns3::UniformRandomVariable[Min=0.0,Max=50.0]"),
                                                               "Z", ConstantValue(0.0));
    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)),
                                "Distance", DoubleValue(5.0));
    ueMobility.Install(uEs);

    NrPointToPointEpcHelper epcHelper;
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.0001)));
    epcHelper.SetPgwApplicationInstaller(p2ph);

    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();

    nrHelper->SetGnbDeviceAttribute("Numerology", UintegerValue(1));
    nrHelper->SetUeDeviceAttribute("Numerology", UintegerValue(1));
    nrHelper->SetSchedulerTypeId(NrMacSchedulerTdmaRR::GetTypeId());

    NetDeviceContainer gnbDevs = nrHelper->InstallGnbDevice(gNbs, epcHelper.GetGnbContainer());
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice(uEs, epcHelper.GetUeContainer());

    epcHelper.AttachToEpc(ueDevs, gnbDevs);

    InternetStackHelper internet;
    internet.Install(uEs);

    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper.AssignUeIpv4Address(ueDevs);

    for (uint32_t i = 0; i < uEs.GetN(); ++i) {
        Ptr<Node> ueNode = uEs.Get(i);
        for (uint32_t j = 0; j < ueNode->GetObject<Ipv4>()->GetNInterfaces(); ++j) {
            if (ueNode->GetObject<Ipv4>()->GetAddress(j, 0).GetLocal() != Ipv4Address::GetZero()) {
                NS_LOG_INFO("UE " << i << " has IP address " << ueNode->GetObject<Ipv4>()->GetAddress(j, 0).GetLocal());
            }
        }
    }

    uint16_t dlPort = 8080;

    PacketSinkHelper dlPacketSink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), dlPort));
    ApplicationContainer sinkApps;

    for (uint32_t i = 0; i < uEs.GetN(); ++i) {
        sinkApps.Add(dlPacketSink.Install(uEs.Get(i)));
    }

    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simTime));

    OnOffHelper clientHelper("ns3::TcpSocketFactory", Address());
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("100Kb/s")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;

    for (uint32_t i = 0; i < uEs.GetN(); ++i) {
        AddressValue remoteAddress(InetSocketAddress(ueIpIfaces.GetAddress((i + 1) % uEs.GetN()), dlPort));
        clientHelper.SetAttribute("Remote", remoteAddress);
        clientApps.Add(clientHelper.Install(uEs.Get(i)));
    }

    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}