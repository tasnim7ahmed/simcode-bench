#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include <iostream>
#include <string>

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Config::SetDefault ("ns3::LteUePhy::HarqProcessesPerUe", UintegerValue (8));

    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(1);

    LteHelper lteHelper;
    lteHelper.SetEnbDeviceAttribute ("DlEarfcn", UintegerValue (100));
    lteHelper.SetEnbDeviceAttribute ("UlEarfcn", UintegerValue (18100));

    Point2D enbPosition(0.0, 0.0);

    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
    enbPositionAlloc->Add(Vector(enbPosition.x, enbPosition.y, 0));
    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.SetPositionAllocator(enbPositionAlloc);
    enbMobility.Install(enbNodes);

    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                 "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
    ueMobility.Install(ueNodes);

    NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice(ueNodes);

    InternetStackHelper internet;
    internet.Install(enbNodes);
    internet.Install(ueNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbLteDevs);
    Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueLteDevs);

    lteHelper.Attach(ueLteDevs, enbLteDevs.Get(0));

    UdpServerHelper server(80);
    ApplicationContainer serverApps = server.Install(enbNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper client(enbIpIface.GetAddress(0), 80);
    client.SetAttribute("MaxPackets", UintegerValue(5));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = client.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}