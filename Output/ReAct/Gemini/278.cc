#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Config::SetDefault ("ns3::LteUePhy::HarqProcessesForPusch", UintegerValue (8));

    NodeContainer enbNodes;
    enbNodes.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(1);

    LteHelper lteHelper;
    lteHelper.SetEnbDeviceAttribute("DlEarfcn", UintegerValue(100));
    lteHelper.SetEnbDeviceAttribute("UlEarfcn", UintegerValue(18100));

    Point2D enbPosition(0.0, 0.0);

    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
    enbPositionAlloc->Add(enbPosition);
    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.SetPositionAllocator(enbPositionAlloc);
    enbMobility.Install(enbNodes);

    MobilityHelper ueMobility;
    ueMobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                    "X", StringValue("25.0"),
                                    "Y", StringValue("25.0"),
                                    "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=25.0]"));
    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Mode", StringValue("Time"),
                                "Time", StringValue("1s"),
                                "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                                "Bounds", StringValue("0|50|0|50"));
    ueMobility.Install(ueNodes);

    NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice(ueNodes);

    lteHelper.Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    InternetStackHelper internet;
    internet.Install(enbNodes);
    internet.Install(ueNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbLteDevs);
    Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueLteDevs);

    uint16_t dlPort = 80;
    UdpServerHelper dlPacketSinkHelper(dlPort);
    ApplicationContainer dlPacketSinkApp = dlPacketSinkHelper.Install(enbNodes.Get(0));
    dlPacketSinkApp.Start(Seconds(0.0));
    dlPacketSinkApp.Stop(Seconds(10.0));

    Ipv4Address remoteAddress = enbIpIface.GetAddress(0);

    uint16_t ulPort = 5000;
    BulkSendHelper ulPacketSourceHelper("ns3::UdpSocketFactory",
                                        InetSocketAddress(remoteAddress, dlPort));
    ulPacketSourceHelper.SetAttribute("MaxBytes", UintegerValue(1024));
    ApplicationContainer ulPacketSourceApp = ulPacketSourceHelper.Install(ueNodes.Get(0));
    ulPacketSourceApp.Start(Seconds(1.0));
    ulPacketSourceApp.Stop(Seconds(6.0));

    Simulator::Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}