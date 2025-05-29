#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/mesh-helper.h"
#include "ns3/lorawan-helper.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-interface-container.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SixLowpanIoTSimpleExample");

int 
main (int argc, char *argv[])
{
    // Simulation parameters
    uint32_t numDevices = 5;
    double simulationTime = 20.0; // seconds
    double packetInterval = 2.0;  // seconds
    uint16_t udpPort = 12345;
    uint32_t packetSize = 50;     // bytes

    // Command line arguments
    CommandLine cmd;
    cmd.AddValue ("numDevices", "Number of IoT devices", numDevices);
    cmd.AddValue ("simulationTime", "Simulation time (seconds)", simulationTime);
    cmd.AddValue ("packetInterval", "Interval between UDP packets (seconds)", packetInterval);
    cmd.Parse (argc, argv);

    LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

    // 1. Create nodes: server + IoT devices
    NodeContainer nodes;
    nodes.Create (numDevices + 1);  // first node is the server

    Ptr<Node> serverNode = nodes.Get (0);
    NodeContainer deviceNodes;
    for (uint32_t i = 1; i <= numDevices; ++i)
    {
        deviceNodes.Add (nodes.Get (i));
    }

    // 2. Mobility (optional, here we make all nodes stationary)
    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (nodes);

    // 3. 802.15.4 devices + channel
    LrWpanHelper lrWpanHelper;
    NetDeviceContainer lrwpanDevs = lrWpanHelper.Install (nodes);

    // Assign each node a PAN ID
    lrWpanHelper.AssociateToPan (lrwpanDevs, 0xCAFE);

    // 4. Install 6LoWPAN on top of 802.15.4
    SixLowPanHelper sixlowpanHelper;
    NetDeviceContainer sixlowpanDevs = sixlowpanHelper.Install (lrwpanDevs);

    // 5. Internet stack
    InternetStackHelper internetv6Helper;
    internetv6Helper.SetIpv4StackInstall (false); // Use only Ipv6
    internetv6Helper.Install (nodes);

    // 6. Assign Ipv6 addresses (use fd00::/64)
    Ipv6AddressHelper ipv6;
    ipv6.SetBase (Ipv6Address ("fd00::"), Ipv6Prefix (64));
    Ipv6InterfaceContainer interfaces = ipv6.Assign (sixlowpanDevs);
    interfaces.SetForwarding (0, true);    // Server is border router
    interfaces.SetDefaultRouteInAllNodes (0);

    // 7. Install UDP Server on the central server node
    UdpServerHelper udpServer (udpPort);
    ApplicationContainer serverApps = udpServer.Install (serverNode);
    serverApps.Start (Seconds (0.0));
    serverApps.Stop (Seconds (simulationTime));

    // 8. Install UDP Clients on IoT Devices
    for (uint32_t i = 0; i < deviceNodes.GetN (); ++i)
    {
        UdpClientHelper udpClient (interfaces.GetAddress (0, 1), udpPort); // server address (first node's global)
        udpClient.SetAttribute ("MaxPackets", UintegerValue (uint32_t (simulationTime / packetInterval)));
        udpClient.SetAttribute ("Interval", TimeValue (Seconds (packetInterval)));
        udpClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
        ApplicationContainer clientApp = udpClient.Install (deviceNodes.Get (i));
        clientApp.Start (Seconds (1.0)); // Start after server
        clientApp.Stop (Seconds (simulationTime));
    }

    // 9. Enable packet captures for debugging/analysis (optional)
    lrWpanHelper.EnablePcapAll (std::string ("sixlowpan-iot"));

    // 10. Run simulation
    Simulator::Stop (Seconds (simulationTime));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}