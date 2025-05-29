#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/dsr-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VanetSimoSumoDsrExample");

int 
main (int argc, char *argv[])
{
    // Set simulation parameters
    uint32_t numVehicles = 10;
    double simulationTime = 80.0; // s

    CommandLine cmd;
    cmd.AddValue ("numVehicles", "Number of vehicle nodes", numVehicles);
    cmd.Parse (argc, argv);

    // Create nodes
    NodeContainer vehicles;
    vehicles.Create (numVehicles);

    // Wi-Fi configuration (802.11p)
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());

    NqosWaveMacHelper wifiMac = NqosWaveMacHelper::Default ();
    Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default ();
    NetDeviceContainer devices = wifi80211p.Install (wifiPhy, wifiMac, vehicles);

    // SUMO/TraceBased Mobility configuration
    MobilityHelper mobility;
    std::string sumoTraceFile = "sumo_mobility.tcl";
    mobility.SetMobilityModel ("ns3::TraceBasedMobilityModel",
                               "TraceFilename", StringValue (sumoTraceFile));
    mobility.Install (vehicles);

    // Install Internet stack & DSR
    InternetStackHelper internet;
    DsrMainHelper dsrMain;
    DsrHelper dsr;
    internet.Install (vehicles);
    dsrMain.Install (dsr, vehicles);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.0.0", "255.255.0.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

    // Set up UDP communication: one source, one sink
    uint16_t port = 4000;
    uint32_t packetSize = 512;
    uint32_t numPackets = 80;
    double interPacketInterval = 1.0; // seconds

    // UDP Server on last vehicle
    UdpServerHelper udpServer (port);
    ApplicationContainer serverApps = udpServer.Install (vehicles.Get (numVehicles - 1));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (simulationTime - 1.0));

    // UDP Client on first vehicle
    UdpClientHelper udpClient (interfaces.GetAddress (numVehicles - 1), port);
    udpClient.SetAttribute ("MaxPackets", UintegerValue (numPackets));
    udpClient.SetAttribute ("Interval", TimeValue (Seconds (interPacketInterval)));
    udpClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
    ApplicationContainer clientApps = udpClient.Install (vehicles.Get (0));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (simulationTime - 1.0));

    // Enable PCAP tracing
    wifiPhy.EnablePcapAll ("vanet-dsr");

    Simulator::Stop (Seconds (simulationTime));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}