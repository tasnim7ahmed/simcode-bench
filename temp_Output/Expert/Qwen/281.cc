#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteUdpRandomWalkSimulation");

int main(int argc, char *argv[]) {
    uint16_t numUes = 3;
    double simDuration = 10.0;
    uint32_t packetCount = 10;
    uint16_t port = 9;

    Config::SetDefault("ns3::UdpClient::MaxPackets", UintegerValue(packetCount));
    Config::SetDefault("ns3::UdpClient::Interval", TimeValue(Seconds(1.0)));
    Config::SetDefault("ns3::UdpClient::PacketSize", UintegerValue(1024));

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetAttribute("UseCa", BooleanValue(false));

    NodeContainer enbNodes;
    enbNodes.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(numUes);

    MobilityHelper mobilityEnb;
    mobilityEnb.SetPositionAllocator("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue(0.0),
                                     "MinY", DoubleValue(0.0),
                                     "DeltaX", DoubleValue(50.0),
                                     "DeltaY", DoubleValue(50.0),
                                     "GridWidth", UintegerValue(1),
                                     "LayoutType", StringValue("RowFirst"));
    mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityEnb.Install(enbNodes);

    MobilityHelper mobilityUe;
    mobilityUe.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)),
                                "Distance", DoubleValue(5.0));
    mobilityUe.Install(ueNodes);

    InternetStackHelper internet;
    internet.Install(enbNodes);
    internet.Install(ueNodes);

    lteHelper->SetSchedulerType("ns3::RrFfMacScheduler");
    lteHelper->SetSpectrumChannelType("ns3::SimpleUeMapChannel");

    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer enbIfs = ipv4.Assign(enbDevs);
    Ipv4InterfaceContainer ueIfs = ipv4.Assign(ueDevs);

    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(enbNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simDuration));

    UdpClientHelper clientHelper(enbIfs.GetAddress(0), port);
    ApplicationContainer clientApps = clientHelper.Install(ueNodes);
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simDuration));

    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}