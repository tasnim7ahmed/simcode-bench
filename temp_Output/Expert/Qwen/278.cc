#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteHttpSimulation");

int main(int argc, char *argv[]) {
    uint16_t numberOfEnb = 1;
    uint16_t numberOfUe = 1;
    double simTime = 10.0;
    uint32_t packetCount = 5;
    uint32_t packetInterval = 1; // seconds

    Config::SetDefault("ns3::UdpSocket::RcvBufSize", UintegerValue(65536 * 1024));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1400));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(131072));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(131072));

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetAttribute("UseCa", BooleanValue(false));

    NodeContainer enbNodes;
    enbNodes.Create(numberOfEnb);

    NodeContainer ueNodes;
    ueNodes.Create(numberOfUe);

    MobilityHelper mobilityEnb;
    mobilityEnb.SetPositionAllocator("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue(0.0),
                                     "MinY", DoubleValue(0.0),
                                     "DeltaX", DoubleValue(100.0),
                                     "DeltaY", DoubleValue(0.0),
                                     "GridWidth", UintegerValue(3),
                                     "LayoutType", StringValue("RowFirst"));
    mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityEnb.Install(enbNodes);

    MobilityHelper mobilityUe;
    mobilityUe.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                    "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                    "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    mobilityUe.SetMobilityModel("ns3::RandomWalk2DMobilityModel",
                                "Mode", StringValue("Time"),
                                "Time", TimeValue(Seconds(1.0)),
                                "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                                "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
    mobilityUe.Install(ueNodes);

    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    NetDeviceContainer enbDevs;
    enbDevs = lteHelper->InstallEnbDevice(enbNodes);

    NetDeviceContainer ueDevs;
    ueDevs = lteHelper->InstallUeDevice(ueNodes);

    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = ipv4.Assign(ueDevs);
    Ipv4InterfaceContainer enbIpIface;
    enbIpIface = ipv4.Assign(enbDevs);

    uint16_t dlPort = 80;

    UdpServerHelper dlServer(dlPort);
    ApplicationContainer serverApps = dlServer.Install(enbNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    UdpClientHelper dlClient(enbIpIface.GetAddress(0), dlPort);
    dlClient.SetAttribute("MaxPackets", UintegerValue(packetCount));
    dlClient.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
    dlClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = dlClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}