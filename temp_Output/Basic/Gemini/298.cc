#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config-store-module.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteHandoverSimulation");

int main(int argc, char *argv[]) {
    Config::SetDefault("ns3::LteEnbPhy::TxPower", DoubleValue(30));
    Config::SetDefault("ns3::UdpClient::Interval", TimeValue(MilliSeconds(10)));
    Config::SetDefault("ns3::UdpClient::MaxPackets", UintegerValue(1000000));
    Config::SetDefault("ns3::LteEnbRrc::HandoverTriggerType", EnumValue(LteEnbRrc::HANDOVER_TRIGGER_BEST_CELL));

    CommandLine cmd;
    cmd.Parse(argc, argv);

    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("LteEnbRrc", LOG_LEVEL_ALL);
    LogComponentEnable("LteUeRrc", LOG_LEVEL_ALL);

    NodeContainer enbNodes;
    enbNodes.Create(2);

    NodeContainer ueNodes;
    ueNodes.Create(1);

    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer remoteHostDevices;
    remoteHostDevices = p2p.Install(remoteHostContainer, enbNodes.Get(0));

    InternetStackHelper internet;
    internet.Install(remoteHostContainer);
    internet.Install(enbNodes);
    internet.Install(ueNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer remoteHostInterface;
    remoteHostInterface = ipv4.Assign(remoteHostDevices);

    Ipv4AddressHelper ipv4Lte;
    ipv4Lte.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer enbLteInterface;
    enbLteInterface = ipv4Lte.Assign(enbNodes);

    Ipv4AddressHelper ipv4Ue;
    ipv4Ue.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer ueLteInterface;
    ueLteInterface = ipv4Ue.Assign(ueNodes);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.Install(enbNodes);

    Ptr<ConstantPositionMobilityModel> enb0Mobility = enbNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
    enb0Mobility->SetPosition(Vector(0.0, 0.0, 0.0));

    Ptr<ConstantPositionMobilityModel> enb1Mobility = enbNodes.Get(1)->GetObject<ConstantPositionMobilityModel>();
    enb1Mobility->SetPosition(Vector(500.0, 0.0, 0.0));

    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Mode", StringValue("Time"),
                                "Time", StringValue("1s"),
                                "Speed", StringValue("10.0"));
    ueMobility.Install(ueNodes);

    Ptr<RandomWalk2dMobilityModel> randomWalk = ueNodes.Get(0)->GetObject<RandomWalk2dMobilityModel>();
    randomWalk->SetBounds(0, 0, 500, 0);

    LteHelper lteHelper;
    lteHelper.SetEnbDeviceAttribute("DlEarfcn", UintegerValue(100));
    lteHelper.SetUeDeviceAttribute("UlEarfcn", UintegerValue(18100));

    NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice(ueNodes);

    lteHelper.Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    UdpServerHelper server(9);
    ApplicationContainer serverApps = server.Install(enbNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper client(enbLteInterface.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}