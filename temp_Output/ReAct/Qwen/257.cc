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
    Config::SetDefault("ns3::UdpClient::MaxPackets", UintegerValue(2));
    Config::SetDefault("ns3::UdpClient::Interval", TimeValue(MilliSeconds(100)));
    Config::SetDefault("ns3::UdpClient::PacketSize", UintegerValue(512));
    Config::SetDefault("ns3::LteHelper::UseIdealRrc", BooleanValue(true));

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    lteHelper->SetAttribute("PathlossModel", StringValue("ns3::FriisSpectrumPropagationLossModel"));

    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(2);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    NetDeviceContainer enbDevs;
    enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs;
    ueDevs = lteHelper->InstallUeDevice(ueNodes);

    epcHelper->AttachToEpc(ueDevs, Ipv4Address());

    InternetStackHelper internet;
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    uint16_t dlPort = 9;
    Address sinkLocalAddr(InetSocketAddress(Ipv4Address::GetAny(), dlPort));
    PacketSinkHelper dlPacketSinkHelper("ns3::UdpSocketFactory", sinkLocalAddr);
    ApplicationContainer dlSinkApp = dlPacketSinkHelper.Install(ueNodes.Get(1));
    dlSinkApp.Start(Seconds(0.0));
    dlSinkApp.Stop(Seconds(simTime));

    InetSocketAddress dlRemoteAddr(ueIpIface.GetAddress(1), dlPort);
    dlRemoteAddr.SetTtl(255);
    UdpClientHelper dlClient(dlRemoteAddr);
    ApplicationContainer clientApp = dlClient.Install(ueNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}