#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiHttpSimpleExample");

int main (int argc, char *argv[])
{
    // Simulation parameters
    double simTime = 10.0;

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Set up Wi-Fi 802.11b
    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    phy.SetChannel (channel.Create ());

    WifiMacHelper mac;
    Ssid ssid = Ssid ("wifi-simple");

    // Install on both as STA (since both are stationary, ad hoc is fine)
    mac.SetType ("ns3::AdhocWifiMac", "Ssid", SsidValue (ssid));
    NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

    // Position nodes in a 5m x 5m grid
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    positionAlloc->Add (Vector (0.0, 0.0, 0.0));
    positionAlloc->Add (Vector (5.0, 0.0, 0.0));
    mobility.SetPositionAllocator (positionAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (nodes);

    // Install the internet stack
    InternetStackHelper internet;
    internet.Install (nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaces = ipv4.Assign (devices);

    // Install a packet sink on node 0 to simulate HTTP server (port 80)
    uint16_t httpPort = 80;
    Address serverAddress (InetSocketAddress (Ipv4Address::GetAny (), httpPort));
    PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApp = packetSinkHelper.Install (nodes.Get (0));
    serverApp.Start (Seconds (1.0));
    serverApp.Stop (Seconds (simTime));

    // OnOffApplication as HTTP client
    OnOffHelper onoff ("ns3::TcpSocketFactory", Address (InetSocketAddress (ifaces.GetAddress (0), httpPort)));
    onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("500kbps")));
    onoff.SetAttribute ("PacketSize", UintegerValue (512)); // Typical HTTP GET size
    onoff.SetAttribute ("MaxBytes", UintegerValue (1024)); // Simulate small HTTP request/response
    ApplicationContainer clientApp = onoff.Install (nodes.Get (1));
    clientApp.Start (Seconds (2.0));
    clientApp.Stop (Seconds (simTime));

    // Enable pcap tracing
    phy.EnablePcap ("wifi-http-sim", devices);

    // Run simulation
    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}