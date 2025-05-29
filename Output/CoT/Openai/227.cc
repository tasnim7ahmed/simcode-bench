#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UAV_Aodv_Wifi");

int main (int argc, char *argv[])
{
    uint32_t nUavs = 5;
    double simTime = 60.0;
    bool verbose = false;

    CommandLine cmd;
    cmd.AddValue ("nUavs", "Number of UAVs", nUavs);
    cmd.AddValue ("verbose", "Enable verbose log", verbose);
    cmd.Parse (argc, argv);

    if (verbose)
    {
        LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
        LogComponentEnable ("AodvRoutingProtocol", LOG_LEVEL_INFO);
    }

    NodeContainer uavNodes;
    uavNodes.Create (nUavs);
    NodeContainer gcsNode;
    gcsNode.Create (1);
    NodeContainer allNodes;
    allNodes.Add (gcsNode);
    allNodes.Add (uavNodes);

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());
    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211g);
    WifiMacHelper wifiMac;
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("ErpOfdmRate54Mbps"), "ControlMode", StringValue ("ErpOfdmRate24Mbps"));

    Ssid ssid = Ssid ("uav-network");
    wifiMac.SetType ("ns3::AdhocWifiMac", "Ssid", SsidValue (ssid));

    NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, allNodes);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    // GCS at fixed position
    positionAlloc->Add (Vector (0.0, 0.0, 0.0));
    mobility.SetPositionAllocator (positionAlloc);

    // UAVs: RandomWaypointMobilityModel in 3D (X:0-200, Y:0-200, Z:50-150)
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (gcsNode);

    MobilityHelper uavMobility;
    uavMobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                     "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=200.0]"),
                                     "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=200.0]"),
                                     "Z", StringValue ("ns3::UniformRandomVariable[Min=50.0|Max=150.0]"));
    uavMobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                                 "Speed", StringValue ("ns3::UniformRandomVariable[Min=5.0|Max=15.0]"),
                                 "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                                 "PositionAllocator", PointerValue (uavMobility.GetPositionAllocator ()));
    uavMobility.Install (uavNodes);

    InternetStackHelper internet;
    AodvHelper aodv;
    internet.SetRoutingHelper (aodv);
    internet.Install (allNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

    uint16_t udpPort = 4000;
    ApplicationContainer serverApps, clientApps;

    // UDP server on GCS
    UdpServerHelper udpServer (udpPort);
    serverApps.Add (udpServer.Install (gcsNode.Get (0)));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (simTime));

    // Each UAV is a UDP client, sending CBR traffic to the GCS
    for (uint32_t i = 0; i < nUavs; ++i)
    {
        UdpClientHelper udpClient (interfaces.GetAddress (0), udpPort);
        udpClient.SetAttribute ("MaxPackets", UintegerValue (1000000));
        udpClient.SetAttribute ("Interval", TimeValue (Seconds (0.2)));
        udpClient.SetAttribute ("PacketSize", UintegerValue (1024));
        clientApps.Add (udpClient.Install (uavNodes.Get (i)));
    }
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (simTime));

    wifiPhy.EnablePcapAll ("uav-aodv-wifi");

    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}