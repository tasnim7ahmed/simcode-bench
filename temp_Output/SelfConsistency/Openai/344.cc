#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiGridTcpSimulation");

int
main (int argc, char *argv[])
{
    // Enable logging if needed
    // LogComponentEnable("WifiGridTcpSimulation", LOG_LEVEL_INFO);

    // Simulation parameters
    uint32_t numNodes = 3;
    double simTime = 20.0; // seconds
    uint32_t packetSize = 512; // bytes
    uint32_t numPackets = 10;
    double interval = 0.1; // seconds
    uint16_t port = 9; // TCP server port
    
    // Create nodes
    NodeContainer wifiNodes;
    wifiNodes.Create (numNodes);

    // Configure WiFi
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetChannel (channel.Create ());

    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211b);
    WifiMacHelper mac;
    Ssid ssid = Ssid ("wifi-grid-ssid");
    mac.SetType ("ns3::StaWifiMac",
                 "Ssid", SsidValue (ssid),
                 "ActiveProbing", BooleanValue (false));
    NetDeviceContainer staDevices = wifi.Install (phy, mac, wifiNodes);

    // All nodes (just 3) are station nodes, set first node as AP for association
    mac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid));
    Ptr<Node> apNode = wifiNodes.Get (2); // Let's use node 2 as the AP as it's also the server
    NetDeviceContainer apDevice = wifi.Install (phy, mac, apNode);

    // Merge all devices for convenience
    NetDeviceContainer wifiDevices;
    for (uint32_t i = 0; i < staDevices.GetN (); ++i)
      wifiDevices.Add (staDevices.Get (i));
    wifiDevices.Add (apDevice.Get (0));

    // Mobility: place nodes in a 3x3 grid, using only three nodes
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue (0.0),
                                  "MinY", DoubleValue (0.0),
                                  "DeltaX", DoubleValue (30.0),
                                  "DeltaY", DoubleValue (30.0),
                                  "GridWidth", UintegerValue (3),
                                  "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiNodes);

    // The AP node as well
    mobility.Install (apNode);

    // Internet stack & IP assignment
    InternetStackHelper stack;
    stack.Install (wifiNodes);
    stack.Install (apNode);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (wifiDevices);
    Ipv4Address serverAddress = interfaces.Get (2);

    // Install server (TCP) on node 2 (AP)
    Address serverBindAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
    PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", serverBindAddress);
    ApplicationContainer serverApp = packetSinkHelper.Install (wifiNodes.Get (2));
    serverApp.Start (Seconds (0.0));
    serverApp.Stop (Seconds (simTime));

    // Install TCP client (OnOffApplication) on nodes 0 and 1
    for (uint32_t i = 0; i < 2; ++i)
    {
        OnOffHelper clientHelper ("ns3::TcpSocketFactory",
                                  Address (InetSocketAddress (serverAddress, port)));
        clientHelper.SetAttribute ("PacketSize", UintegerValue (packetSize));
        clientHelper.SetAttribute ("MaxBytes", UintegerValue (packetSize * numPackets));
        clientHelper.SetAttribute ("DataRate", DataRateValue (DataRate ("10Mbps")));
        clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
        clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
        clientHelper.SetAttribute ("StartTime", TimeValue (Seconds (1.0 + i * 0.1)));
        clientHelper.SetAttribute ("StopTime", TimeValue (Seconds (simTime)));
        clientHelper.SetAttribute ("Interval", TimeValue (Seconds (interval)));

        ApplicationContainer clientApp = clientHelper.Install (wifiNodes.Get (i));
        clientApp.Start (Seconds (1.0 + i * 0.1));
        clientApp.Stop (Seconds (simTime));
    }

    // Enable PCAP tracing (optional)
    phy.EnablePcapAll ("wifi-grid-tcp");

    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}