#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MeshTopology4Nodes");

int main (int argc, char *argv[])
{
    LogComponentEnable ("MeshTopology4Nodes", LOG_LEVEL_INFO);
    
    // Create 4 nodes
    NodeContainer nodes;
    nodes.Create (4);
    
    // Set up Wi-Fi mesh
    WifiHelper wifi;
    MeshHelper mesh;
    mesh.SetStackInstaller ("ns3::Dot11sStack");  // 802.11s for mesh
    mesh.SetSpreadInterfaceChannels (MeshHelper::SPREAD_CHANNELS);
    mesh.SetMacType ("RandomStart", TimeValue (Seconds (0.1)));
    NetDeviceContainer meshDevices = mesh.Install (wifi, nodes);
    
    // Position nodes in a 2x2 grid layout
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue (0.0),
                                   "MinY", DoubleValue (0.0),
                                   "DeltaX", DoubleValue (50.0),   // X distance between nodes
                                   "DeltaY", DoubleValue (50.0),   // Y distance between nodes
                                   "GridWidth", UintegerValue (2), // Two columns for 2x2 grid
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
    
    // Create UDP Echo server on node 0
    UdpEchoServerHelper echoServer (9);  // Port number
    ApplicationContainer serverApps = echoServer.Install (nodes.Get (0));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (15.0));
    
    // Create UDP Echo client on node 1 to send data to node 0
    UdpEchoClientHelper echoClient (interfaces.GetAddress (0), 9);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (20));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
    ApplicationContainer clientApps = echoClient.Install (nodes.Get (1));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (15.0));
    
    // Create another UDP Echo client on node 2 to send data to node 3
    UdpEchoClientHelper echoClient2 (interfaces.GetAddress (3), 9);
    echoClient2.SetAttribute ("MaxPackets", UintegerValue (20));
    echoClient2.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    echoClient2.SetAttribute ("PacketSize", UintegerValue (1024));
    ApplicationContainer clientApps2 = echoClient2.Install (nodes.Get (2));
    clientApps2.Start (Seconds (3.0));
    clientApps2.Stop (Seconds (15.0));
    
    // Enable PCAP tracing to capture packet flow
    wifi.EnablePcapAll ("mesh-topology-4-nodes");

    // Run the simulation
    Simulator::Run ();
    
    // Print basic statistics
    std::cout << "Mesh simulation with 4 nodes finished." << std::endl;

    Simulator::Destroy ();
    return 0;
}

