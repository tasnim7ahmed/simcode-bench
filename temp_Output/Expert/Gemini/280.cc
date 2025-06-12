#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include <iostream>

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Config::SetDefault ("ns3::UdpClient::Interval", TimeValue (MilliSeconds (10)));
    Config::SetDefault ("ns3::UdpClient::MaxPackets", UintegerValue (1000000));

    NodeContainer gnbNodes;
    gnbNodes.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(2);

    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                  "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                  "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                  "Z", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
    mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                               "Mode", StringValue ("Time"),
                               "Time", StringValue ("2s"),
                               "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
    mobility.Install(ueNodes);

    Ptr<ListPositionAllocator> gnbPositionAlloc = CreateObject<ListPositionAllocator> ();
    gnbPositionAlloc->Add (Vector (25.0, 25.0, 0.0));
    mobility.SetPositionAllocator (gnbPositionAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install(gnbNodes);

    NetDeviceContainer gnbDevs;
    NetDeviceContainer ueDevs;
    NrHelper nrHelper;
    nrHelper.SetGnbAntennaModelType ("ns3::IsotropicAntennaModel");
    nrHelper.SetUeAntennaModelType ("ns3::IsotropicAntennaModel");
    nrHelper.AddGnb(gnbNodes);
    ueDevs = nrHelper.InstallUeDevice(ueNodes);
    gnbDevs = nrHelper.InstallGnbDevice(gnbNodes);

    InternetStackHelper internet;
    internet.Install(gnbNodes);
    internet.Install(ueNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer gnbIpIface = ipv4.Assign(gnbDevs);
    Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevs);

    UdpServerHelper server(8080);
    ApplicationContainer serverApps = server.Install(ueNodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper client(ueIpIface.GetAddress(1), 8080);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = client.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    nrHelper.EnableTraces();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}