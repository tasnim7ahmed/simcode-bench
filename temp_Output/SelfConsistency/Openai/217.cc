#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include <fstream>
#include <iostream>

using namespace ns3;

// Trace file for mobility events
static std::ofstream mobilityTraceFile;

// Function to log node positions (mobility trace)
void
MobilityTrace (Ptr<const MobilityModel> mobility, uint32_t nodeId)
{
    Vector pos = mobility->GetPosition ();
    mobilityTraceFile << Simulator::Now ().GetSeconds () << " Node: " << nodeId
                      << " Position: (" << pos.x << ", " << pos.y << ", " << pos.z << ")\n";
}

// Callback for received packets at the receiver
struct PacketStats
{
    uint32_t rxPackets = 0;
    uint32_t txPackets = 0;
};

void
RxPacket (Ptr<Packet> packet, const Address &address, PacketStats *stats)
{
    stats->rxPackets++;
}

void
TxPacket (Ptr<const Packet> packet, PacketStats *stats)
{
    stats->txPackets++;
}

int
main (int argc, char *argv[])
{
    // Simulation parameters
    uint32_t numNodes = 5;
    double simTime = 60.0; // seconds
    double txRange = 100.0; // Transmission range in meters
    double areaSize = 200.0; // Simulation area (meters)
    std::string mobilityTraceFilename = "mobility-trace.log";

    // Command line arguments
    CommandLine cmd;
    cmd.AddValue ("numNodes", "Number of nodes in the MANET", numNodes);
    cmd.AddValue ("simTime", "Total duration of the simulation (s)", simTime);
    cmd.AddValue ("areaSize", "Simulation area range (meters)", areaSize);
    cmd.AddValue ("mobilityTrace", "Mobility trace output file", mobilityTraceFilename);
    cmd.Parse (argc, argv);

    // Open mobility trace file
    mobilityTraceFile.open (mobilityTraceFilename, std::ios_base::out);

    // 1. Create nodes
    NodeContainer nodes;
    nodes.Create (numNodes);

    // 2. Install mobility model
    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                               "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                               "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                               "PositionAllocator",
                               StringValue ("ns3::RandomRectanglePositionAllocator[MinX=0|MinY=0|MaxX="
                                             + std::to_string(areaSize) + "|MaxY=" + std::to_string(areaSize) + "]"));
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue (0.0),
                                   "MinY", DoubleValue (0.0),
                                   "DeltaX", DoubleValue (20.0),
                                   "DeltaY", DoubleValue (20.0),
                                   "GridWidth", UintegerValue (3),
                                   "LayoutType", StringValue ("RowFirst"));
    mobility.Install (nodes);

    // Attach mobility trace
    for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
        Ptr<MobilityModel> mob = nodes.Get (i)->GetObject<MobilityModel> ();
        Simulator::Schedule (Seconds (0.1), &MobilityTrace, mob, i);
    }

    // Trace periodically
    for (double t = 0.0; t <= simTime; t += 1.0)
    {
        for (uint32_t i = 0; i < nodes.GetN (); ++i)
        {
            Ptr<MobilityModel> mob = nodes.Get (i)->GetObject<MobilityModel> ();
            Simulator::Schedule (Seconds (t), &MobilityTrace, mob, i);
        }
    }

    // 3. Create wireless devices (WiFi)
    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();

    wifiPhy.SetChannel (wifiChannel.Create ());
    wifiPhy.Set ("TxPowerStart", DoubleValue (16.0));
    wifiPhy.Set ("TxPowerEnd", DoubleValue (16.0));
    wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue (-89.0));
    wifiPhy.Set ("CcaMode1Threshold", DoubleValue (-62.0));

    WifiMacHelper wifiMac;
    wifiMac.SetType ("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

    // 4. Internet stack + AODV
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper (aodv);
    internet.Install (nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.0.0", "255.255.0.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

    // 5. Set up CBR application between node 0 and node N-1
    uint16_t port = 9000;
    double appStart = 1.0, appStop = simTime - 1.0;

    // UDP Server (sink)
    UdpServerHelper server (port);
    ApplicationContainer serverApp = server.Install (nodes.Get (numNodes - 1));
    serverApp.Start (Seconds (appStart));
    serverApp.Stop (Seconds (appStop));

    // UDP Client (source)
    UdpClientHelper client (interfaces.GetAddress (numNodes - 1), port);
    client.SetAttribute ("MaxPackets", UintegerValue (3200)); // Large enough
    client.SetAttribute ("Interval", TimeValue (MilliSeconds (50)));
    client.SetAttribute ("PacketSize", UintegerValue (512));
    ApplicationContainer clientApp = client.Install (nodes.Get (0));
    clientApp.Start (Seconds (appStart));
    clientApp.Stop (Seconds (appStop));

    // 6. Install packet delivery statistics tracking
    PacketStats appStats;
    Config::ConnectWithoutContext ("/NodeList/0/ApplicationList/0/$ns3::UdpClient/Tx", MakeBoundCallback (&TxPacket, &appStats));
    Config::ConnectWithoutContext ("/NodeList/" + std::to_string(numNodes - 1) + "/ApplicationList/0/$ns3::UdpServer/Rx", MakeBoundCallback (&RxPacket, &appStats));

    // Enable pcap tracing (optional, for wifi)
    wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
    wifiPhy.EnablePcap ("manet-aodv", devices);

    // Run
    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();
    Simulator::Destroy ();

    mobilityTraceFile.close ();

    // Print statistics
    std::cout << "Total Packets Sent: " << appStats.txPackets << std::endl;
    std::cout << "Total Packets Received: " << appStats.rxPackets << std::endl;
    double pdr = (appStats.txPackets > 0) ? (double)appStats.rxPackets / appStats.txPackets : 0.0;
    std::cout << "Packet Delivery Ratio (PDR): " << pdr * 100.0 << " %" << std::endl;

    return 0;
}