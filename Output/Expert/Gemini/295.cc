#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("LteUdpExample", LOG_LEVEL_INFO);

    NodeContainer enbNodes;
    enbNodes.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(1);

    LteHelper lteHelper;
    lteHelper.SetEnbAntennaModelType("ns3::CosineAntennaModel");
    lteHelper.SetLteAttributes ("UeTransmissionPowerModel", StringValue ("ns3::ConstantUeTxPowerModel"));
    lteHelper.AddEnb(enbNodes.Get(0));

    NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice(ueNodes);

    lteHelper.Attach(ueLteDevs, enbLteDevs.Get(0));

    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.Install(enbNodes);

    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                 "Bounds", RectangleValue(Rectangle(-100, 100, -100, 100)));
    ueMobility.Install(ueNodes);

    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueLteDevs);
    Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbLteDevs);

    UdpServerHelper server(9);
    ApplicationContainer serverApps = server.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper client(ueIpIface.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(enbNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}