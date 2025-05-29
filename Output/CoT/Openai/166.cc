#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-socket.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocAodvSimulation");

int main (int argc, char *argv[])
{
    uint32_t numNodes = 5;
    double simTime = 20.0;
    double areaSize = 100.0;

    // Enable logging for AODV and UDP
    LogComponentEnable ("AodvRoutingProtocol", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create (numNodes);

    // Wifi configuration
    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());

    WifiMacHelper wifiMac;
    wifiMac.SetType ("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

    // Mobility - random placement in a 100x100 area
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (nodes);

    // Internet stack with AODV
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper (aodv);
    internet.Install (nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

    // Create UDP server on node 4
    uint16_t serverPort = 4000;
    UdpServerHelper udpServer (serverPort);
    ApplicationContainer serverApps = udpServer.Install (nodes.Get (4));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (simTime));

    // Create UDP client on node 0, send to server
    UdpClientHelper udpClient (interfaces.GetAddress (4), serverPort);
    udpClient.SetAttribute ("MaxPackets", UintegerValue (1000));
    udpClient.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
    udpClient.SetAttribute ("PacketSize", UintegerValue (512));

    ApplicationContainer clientApps = udpClient.Install (nodes.Get (0));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (simTime));

    // Enable packet tracing
    wifiPhy.EnablePcap ("adhoc-aodv", devices);

    // Flow monitor to observe paths and packet flow
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();

    monitor->SerializeToXmlFile ("adhoc-aodv-flowmon.xml", true, true);

    Simulator::Destroy ();
    return 0;
}