#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mesh-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv6.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/sixlowpan-helper.h"
#include "ns3/lr-wpan-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SixLowpanIotDevicesExample");

int main (int argc, char *argv[])
{
    uint32_t nIoTDevices = 5;
    double simTime = 20.0; // seconds
    uint16_t serverPort = 5683;
    double packetInterval = 2.0; // seconds
    uint32_t packetSize = 40;
    uint32_t packetCount = 5;

    CommandLine cmd;
    cmd.AddValue ("nIoTDevices", "Number of IoT devices", nIoTDevices);
    cmd.AddValue ("simTime", "Simulation time in seconds", simTime);
    cmd.Parse (argc, argv);

    // Create nodes: IoT devices + 1 server
    NodeContainer allNodes;
    allNodes.Create (nIoTDevices + 1);

    // Distinguish IoT and server nodes
    NodeContainer iotNodes;
    iotNodes.Add (allNodes.GetN () > 1 ? allNodes.Get (0) : 0);
    for (uint32_t i = 0; i < nIoTDevices; ++i)
    {
        iotNodes.Add (allNodes.Get (i));
    }
    Ptr<Node> serverNode = allNodes.Get (nIoTDevices);

    // Create LR-WPAN devices & helper
    LrWpanHelper lrWpanHelper;
    NetDeviceContainer lrwpanDevs = lrWpanHelper.Install (allNodes);

    // Assign a common PAN ID
    lrWpanHelper.AssociateToPan (lrwpanDevs, 0x1234);

    // Install 6LoWPAN adaptation layer over LR-WPAN
    SixLowPanHelper sixlowpanHelper;
    NetDeviceContainer sixlowpanDevs = sixlowpanHelper.Install (lrwpanDevs);

    // Install Internet stack (IPv6 only)
    InternetStackHelper internetv6;
    internetv6.SetIpv4StackInstall (false);
    internetv6.Install (allNodes);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
    Ipv6InterfaceContainer ifaces = ipv6.Assign (sixlowpanDevs);
    ifaces.SetForwarding (0, true);
    ifaces.SetDefaultRouteInAllNodes (0);

    // Server listens on last node
    UdpServerHelper udpServer (serverPort);
    ApplicationContainer serverApps = udpServer.Install (serverNode);
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (simTime));

    // Each IoT device sends UDP packets to the server
    for (uint32_t i = 0; i < nIoTDevices; ++i)
    {
        UdpClientHelper udpClient (ifaces.GetAddress (nIoTDevices, 1), serverPort);
        udpClient.SetAttribute ("MaxPackets", UintegerValue (packetCount));
        udpClient.SetAttribute ("Interval", TimeValue (Seconds (packetInterval)));
        udpClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
        ApplicationContainer clientApp = udpClient.Install (allNodes.Get (i));
        clientApp.Start (Seconds (2.0 + i)); // stagger starts slightly
        clientApp.Stop (Seconds (simTime));
    }

    // Mobility (optional - static grid)
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    for (uint32_t i = 0; i < nIoTDevices; ++i)
    {
        positionAlloc->Add (Vector (5.0 * i, 5.0, 0.0));
    }
    positionAlloc->Add (Vector (10.0, 20.0, 0.0)); // Server further away
    mobility.SetPositionAllocator (positionAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (allNodes);

    // Enable pcap tracing for one device and server
    lrWpanHelper.EnablePcap ("iot-device", lrwpanDevs.Get (0), true, true);
    lrWpanHelper.EnablePcap ("server", lrwpanDevs.Get (nIoTDevices), true, true);

    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}