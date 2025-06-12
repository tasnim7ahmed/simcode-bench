#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    double simTime = 10.0;
    Config::SetDefault("ns3::UdpClient::Interval", TimeValue(Seconds(0.01)));
    Config::SetDefault("ns3::UdpClient::PacketSize", UintegerValue(1024));
    Config::SetDefault("ns3::LteHelper::UseRlcSm", BooleanValue(true));

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    lteHelper->SetAttribute("PathlossModel", StringValue("ns3::FriisSpectrumPropagationLossModel"));

    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(1);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    uint16_t dlPort = 1234;
    uint16_t ulPort = 8000;
    uint16_t otherPort = 8888;

    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), dlPort));
    PacketSinkHelper dlPacketSinkHelper("ns3::UdpSocketFactory", sinkLocalAddress);
    ApplicationContainer dlSinkApps = dlPacketSinkHelper.Install(ueNodes.Get(0));
    dlSinkApps.Start(Seconds(1.0));
    dlSinkApps.Stop(Seconds(simTime));

    Address sinkRemoteAddress(InetSocketAddress(ueIpIfaces.GetAddress(0), dlPort));
    InetSocketAddress remoteAddress = InetSocketAddress(Ipv4Address::GetBroadcast(), dlPort);
    remoteAddress.SetTos(0xb8);
    UdpClientHelper dlClient(sinkRemoteAddress.GetIpv4(), dlPort);
    dlClient.SetAttribute("MaxPackets", UintegerValue(1000));
    dlClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    dlClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = dlClient.Install(enbNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}