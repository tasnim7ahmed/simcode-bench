#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include <iostream>

using namespace ns3;

int main(int argc, char *argv[]) {

    CommandLine cmd;
    cmd.Parse(argc, argv);

    ConfigStore inputConfig;
    inputConfig.ConfigureDefaults();

    NodeContainer enbNodes;
    enbNodes.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Configure LTE Helper
    LteHelper lteHelper;
    lteHelper.SetEnbAntennaModelType("ns3::CosineAntennaModel");

    // Create eNB and UE devices
    NetDeviceContainer enbDevs = lteHelper.InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper.InstallUeDevice(ueNodes);

    // Mobility model
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Mode", StringValue("Time"),
                              "Time", StringValue("1s"),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(ueNodes);

    Ptr<ConstantPositionMobilityModel> enbMobility = CreateObject<ConstantPositionMobilityModel>();
    enbNodes.Get(0)->AggregateObject(enbMobility);
    Vector3D pos(50,50,0);
    enbMobility->SetPosition(pos);

    // Install internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueNodes);
    Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbNodes);

    // Attach UE to eNB
    lteHelper.Attach(ueDevs, enbDevs.Get(0));

    // Activate default EPS bearer
    enum EpsBearer::Qci q = EpsBearer::GBR_CONV_VOICE;
    EpsBearer bearer(q);
    lteHelper.ActivateDedicatedBearer(ueDevs, bearer, EpcTft::Default());

    // Create UDP application:  eNB (server) and UE (client)
    uint16_t port = 9; // Discard port (RFC 863)
    UdpServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(enbNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper echoClient(enbIpIface.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = echoClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}