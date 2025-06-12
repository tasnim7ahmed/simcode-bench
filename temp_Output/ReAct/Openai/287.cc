#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/mmwave-helper.h"
#include "ns3/mmwave-point-to-point-epc-helper.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MmwaveNrGnbUeUdpExample");

int main (int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse (argc, argv);

    // Create nodes: 1 gNB, 2 UEs
    NodeContainer ueNodes;
    ueNodes.Create (2);
    Ptr<Node> ue0 = ueNodes.Get(0);
    Ptr<Node> ue1 = ueNodes.Get(1);

    NodeContainer gnbNodes;
    gnbNodes.Create (1);

    // Create the NR/EPC core network
    Ptr<MmWavePointToPointEpcHelper> epcHelper = CreateObject<MmWavePointToPointEpcHelper> ();
    Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper> ();
    mmwaveHelper->SetEpcHelper (epcHelper);
    mmwaveHelper->SetSchedulerType ("ns3::MmWaveFlexTtiMacScheduler");
    mmwaveHelper->SetAttribute ("PathlossModel", StringValue ("ns3::ThreeGppUrbanMacroBuildingsPropagationLossModel"));

    // Install Internet stack to all nodes
    InternetStackHelper internet;
    internet.Install (gnbNodes);
    internet.Install (ueNodes);

    // Create gNB device
    NetDeviceContainer gnbDevs = mmwaveHelper->InstallGnbDevice (gnbNodes);

    // Create UE devices
    NetDeviceContainer ueDevs = mmwaveHelper->InstallUeDevice (ueNodes);

    // Mobility: gNB fixed, UEs random in 50x50
    MobilityHelper gnbMobility;
    gnbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    gnbMobility.Install (gnbNodes);
    Ptr<MobilityModel> gnbMob = gnbNodes.Get (0)->GetObject<MobilityModel> ();
    gnbMob->SetPosition (Vector (25.0, 25.0, 1.5));

    MobilityHelper ueMobility;
    ueMobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                    "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                    "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    ueMobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                "Mode", StringValue ("Time"),
                                "Time", TimeValue (Seconds (0.1)),
                                "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=2.0]"),
                                "Bounds", RectangleValue (Rectangle (0.0, 50.0, 0.0, 50.0)));
    ueMobility.Install (ueNodes);

    // Assign IP addresses
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueDevs));

    // Attach UEs to gNB
    mmwaveHelper->AttachToClosestEnb (ueDevs, gnbDevs);

    // Set default gateway for UEs
    for (uint32_t i = 0; i < ueNodes.GetN (); ++i)
    {
        Ptr<Node> ueNode = ueNodes.Get(i);
        Ptr<Ipv4> ipv4 = ueNode->GetObject<Ipv4> ();
        Ipv4StaticRoutingHelper ipv4RoutingHelper;
        Ptr<Ipv4StaticRouting> staticRouting = ipv4RoutingHelper.GetStaticRouting (ipv4);
        staticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

    // UDP server on UE0, listening to port 5001
    uint16_t serverPort = 5001;
    UdpServerHelper udpServer (serverPort);
    ApplicationContainer serverApp = udpServer.Install (ueNodes.Get (0));
    serverApp.Start (Seconds (0.1));
    serverApp.Stop (Seconds (10.0));

    // UDP client on UE1, sending packets to UE0
    UdpClientHelper udpClient (ueIpIface.GetAddress (0), serverPort);
    udpClient.SetAttribute ("MaxPackets", UintegerValue (10000));
    udpClient.SetAttribute ("Interval", TimeValue (MilliSeconds (10)));
    udpClient.SetAttribute ("PacketSize", UintegerValue (1024));
    ApplicationContainer clientApps = udpClient.Install (ueNodes.Get (1));
    clientApps.Start (Seconds (1.0));
    clientApps.Stop (Seconds (10.0));

    // Enable PCAP tracing
    mmwaveHelper->EnableTraces ();
    mmwaveHelper->EnablePcapAll ("mmwave-gnb-ue-udp");

    Simulator::Stop (Seconds (10.0));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}