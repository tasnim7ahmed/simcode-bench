#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MeshTopologyExample");

int main (int argc, char *argv[])
{
    LogComponentEnable ("MeshTopologyExample", LOG_LEVEL_INFO);
    
    // Create 3 nodes
    NodeContainer nodes;
    nodes.Create (3);
    
    // Set up Wi-Fi mesh
    WifiHelper wifi;
    MeshHelper mesh;
    mesh.SetStackInstaller ("ns3::Dot11sStack");  // 802.11s for mesh
    mesh.SetSpreadInterfaceChannels (MeshHelper::SPREAD_CHANNELS);
    mesh.SetMacType ("RandomStart", TimeValue (Seconds (0.1)));
    NetDeviceContainer meshDevices = mesh.Install (wifi, nodes);
    
    // Position nodes in a simple line (static positions)
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue (0.0),
                                   "MinY", DoubleValue (0.0),
                                   "DeltaX", DoubleValue (100.0),
                                   "DeltaY", DoubleValue (0.0),
                                   "GridWidth", UintegerValue (3),
                                   "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (nodes);
    
    // Install internet stack
    InternetStackHelper internet;
    internet.Install (nodes);
    
    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase ("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (meshDevices);
    
    // Create UDP Echo server on node 1
    UdpEchoServerHelper echoServer (9);  // port number
    ApplicationContainer serverApps = echoServer.Install (nodes.Get (0));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (10.0));
    
    // Create UDP Echo client on node 2 to send data to node 1
    UdpEchoClientHelper echoClient (interfaces.GetAddress (0), 9);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
    ApplicationContainer clientApps = echoClient.Install (nodes.Get (1));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (10.0));
    
    // Enable PCAP tracing to capture packet flow
    wifi.EnablePcapAll ("mesh-topology");

    // Run the simulation
    Simulator::Run ();
    
    // Print statistics (transmissions and receptions)
    std::cout << "Mesh simulation finished." << std::endl;

    Simulator::Destroy ();
    return 0;
}

