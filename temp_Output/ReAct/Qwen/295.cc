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
    uint32_t packetSize = 1024;
    Time interPacketInterval = MilliSeconds(100);

    Config::SetDefault("ns3::LteHelper::UseRlcSm", BooleanValue(true));
    Config::SetDefault("ns3::UdpClient::MaxPackets", UintegerValue(1000000));
    Config::SetDefault("ns3::UdpClient::Interval", TimeValue(interPacketInterval));
    Config::SetDefault("ns3::UdpClient::PacketSize", UintegerValue(packetSize));

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetSchedulerType("ns3::PfFfMacScheduler");

    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(1);

    MobilityHelper mobilityEnb;
    mobilityEnb.SetPositionAllocator("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue(0.0),
                                     "MinY", DoubleValue(0.0),
                                     "DeltaX", DoubleValue(0.0),
                                     "DeltaY", DoubleValue(0.0),
                                     "GridWidth", UintegerValue(1),
                                     "LayoutType", StringValue("RowFirst"));
    mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityEnb.Install(enbNodes);

    MobilityHelper mobilityUe;
    mobilityUe.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                    "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                    "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobilityUe.SetMobilityModel("ns3::RandomWalk2DMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobilityUe.Install(ueNodes);

    NetDeviceContainer enbDevs;
    enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs;
    ueDevs = lteHelper->InstallUeDevice(ueNodes);

    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = ipv4h.Assign(ueDevs);
    ipv4h.NewNetwork();
    Ipv4InterfaceContainer enbIpIface;
    enbIpIface = ipv4h.Assign(enbDevs);

    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    uint16_t dlPort = 1234;
    UdpServerHelper dlServer(dlPort);
    ApplicationContainer serverApps = dlServer.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    UdpClientHelper dlClient(ueIpIface.GetAddress(0), dlPort);
    dlClient.SetAttribute("MaxPackets", UintegerValue((simTime * 1000) / interPacketInterval.GetMilliSeconds()));
    dlClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    dlClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    ApplicationContainer clientApps = dlClient.Install(enbNodes.Get(0));
    clientApps.Start(Seconds(0.1));
    clientApps.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}