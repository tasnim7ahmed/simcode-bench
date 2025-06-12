#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t packetsToSend = 1000;
    uint32_t packetSize = 1024;
    double interPacketInterval = 0.01; // seconds
    double simDuration = 10.0;
    double serverStart = 1.0;
    double clientStart = 2.0;

    Config::SetDefault("ns3::LteHelper::UseIdealRrc", BooleanValue(true));
    Config::SetDefault("ns3::LteHelper::UseIdealRlc", BooleanValue(true));

    NodeContainer enbNodes;
    enbNodes.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(1);

    Ptr<ListPositionAllocator> positionAllocEnb = CreateObject<ListPositionAllocator>();
    positionAllocEnb->Add(Vector(0.0, 0.0, 0.0));
    MobilityModelHelper mobilityEnb;
    mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityEnb.Install(enbNodes);
    enbNodes.Get(0)->AggregateObject(positionAllocEnb);

    MobilityModelHelper mobilityUe;
    mobilityUe.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityUe.Install(ueNodes);
    ueNodes.Get(0)->AggregateObject(CreateObject<ListPositionAllocator>()->Add(Vector(10.0, 0.0, 0.0)));

    InternetStackHelper internet;
    internet.Install(enbNodes);
    internet.Install(ueNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    LteHelper lteHelper;
    NetDeviceContainer enbDevs = lteHelper.InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper.InstallUeDevice(ueNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer enbIfs;
    Ipv4InterfaceContainer ueIfs;

    enbIfs = ipv4.Assign(enbDevs);
    ueIfs = ipv4.Assign(ueDevs);

    UdpServerHelper server(9);
    ApplicationContainer serverApp = server.Install(enbNodes.Get(0));
    serverApp.Start(Seconds(serverStart));
    serverApp.Stop(Seconds(simDuration));

    UdpClientHelper client(ueIfs.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(packetsToSend));
    client.SetAttribute("Interval", TimeValue(Seconds(interPacketInterval)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp = client.Install(ueNodes.Get(0));
    clientApp.Start(Seconds(clientStart));
    clientApp.Stop(Seconds(simDuration));

    lteHelper.Attach(ueDevs, enbDevs.Get(0));

    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}