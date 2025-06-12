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
    uint16_t simTime = 10;
    uint16_t gNbNum = 1;
    uint16_t ueNum = 2;

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(8 * 1024 * 1024));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(8 * 1024 * 1024));

    NodeContainer gNbs;
    gNbs.Create(gNbNum);

    NodeContainer ues;
    ues.Create(ueNum);

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // gNodeB at origin

    for (uint16_t i = 0; i < ueNum; ++i) {
        positionAlloc->Add(Vector(50.0, 50.0, 0.0));
    }

    MobilityHelper mobility;
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gNbs);

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)),
                              "Distance", DoubleValue(1.0));
    mobility.Install(ues);

    NrHelper nr;
    nr.SetChannelConditionModelType("ns3::ThreeGppUmiStreetCanyonChannelConditionModel");
    nr.SetPathlossModelType("ns3::ThreeGppUmiPropagationLossModel");

    BandwidthPartInfoPtrVector allBwParts = nr.CreateBandwidthParts(1, BAND_3_5GHZ, 20e6);

    NetDeviceContainer gnbNetDevs = nr.InstallGnbDevice(gNbs, allBwParts);
    NetDeviceContainer ueNetDevs = nr.InstallUeDevice(ues, allBwParts);

    InternetStackHelper internet;
    internet.Install(gNbs);
    internet.Install(ues);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.0.0", "255.255.255.0");

    for (auto it = ueNetDevs.Begin(); it != ueNetDevs.End(); ++it) {
        Ipv4InterfaceContainer ueIpIface = ipv4.Assign(*it);
    }

    for (auto it = gnbNetDevs.Begin(); it != gnbNetDevs.End(); ++it) {
        Ipv4InterfaceContainer gnbIpIface = ipv4.Assign(*it);
    }

    PointToPointEpcHelper epc;
    nr.AttachToEpc(ues, epc.GetSgwPgwNode());

    uint16_t dlPort = 8080;
    Address serverAddr(Ipv4Address::GetAny(), dlPort);

    PacketSinkHelper sink("ns3::TcpSocketFactory", serverAddr);
    ApplicationContainer sinkApp = sink.Install(ues.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simTime));

    OnOffHelper client("ns3::TcpSocketFactory", serverAddr);
    client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    client.SetAttribute("DataRate", DataRateValue(DataRate("100Kbps")));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(ues.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}