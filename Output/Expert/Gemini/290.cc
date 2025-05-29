#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

    CommandLine cmd;
    cmd.Parse(argc, argv);

    Config::SetDefault ("ns3::LteUePhy::EnableUplinkPowerControl", BooleanValue (false));

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();

    NodeContainer enbNodes;
    enbNodes.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(1);

    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.Install(enbNodes);

    MobilityHelper ueMobility;
    ueMobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                     "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                     "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                     "Z", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
    ueMobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                 "Bounds", RectangleValue (Rectangle (0, 100, 0, 100)));
    ueMobility.Install (ueNodes);

    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

    lteHelper->Attach (ueLteDevs, enbLteDevs.Get(0));

    InternetStackHelper internet;
    internet.Install (enbNodes);
    internet.Install (ueNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer enbIpIface = ipv4.Assign (enbLteDevs);
    Ipv4InterfaceContainer ueIpIface = ipv4.Assign (ueLteDevs);

    UdpServerHelper server (5000);
    ApplicationContainer serverApps = server.Install (ueNodes.Get(0));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (10.0));

    UdpClientHelper client (ueIpIface.GetAddress (0), 5000);
    client.SetAttribute ("MaxPackets", UintegerValue (49));
    client.SetAttribute ("Interval", TimeValue (MilliSeconds (200)));
    client.SetAttribute ("PacketSize", UintegerValue (1024));

    ApplicationContainer clientApps = client.Install (enbNodes.Get(0));
    clientApps.Start (Seconds (1.0));
    clientApps.Stop (Seconds (10.0));

    Ptr<RadioBearerStatsCalculator> rlcStats = CreateObject<RadioBearerStatsCalculator> ();
    rlcStats->SetAttribute ("EpochDuration", TimeValue (Seconds (0.1)));
    rlcStats->Install (ueLteDevs, enbLteDevs);

    Simulator::Stop (Seconds (10.0));

    lteHelper->EnablePhyTraces ();
    lteHelper->EnableMacTraces ();
    lteHelper->EnableRlcTraces ();

    AnimationInterface anim("lte-simulation.xml");
    anim.SetConstantPosition(enbNodes.Get(0), 50.0, 50.0);

    Simulator::Run ();

    Simulator::Destroy ();
    return 0;
}