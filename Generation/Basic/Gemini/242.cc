#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/dot15d4-helper.h"
#include "ns3/sixlowpan-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("IoT6LowPanSimulation");

int main(int argc, char *argv[])
{
    // 1. Command line arguments for flexibility
    uint32_t numIoTDevices = 5;
    double simTime = 30.0;   // seconds
    uint16_t serverPort = 9; // Standard discard port for UDP server

    CommandLine cmd(__FILE__);
    cmd.AddValue("numIoTDevices", "Number of IoT client devices", numIoTDevices);
    cmd.AddValue("simTime", "Total simulation time in seconds", simTime);
    cmd.Parse(argc, argv);

    // Optional: Enable logging for specific modules for debugging
    // LogComponentEnable("IoT6LowPanSimulation", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    // LogComponentEnable("SixLowPanIpv6L3Protocol", LOG_LEVEL_ALL);
    // LogComponentEnable("Dot15D4Lrc", LOG_LEVEL_ALL);

    // 2. Create Nodes: one central server and multiple IoT devices
    Ptr<Node> serverNode = CreateObject<Node>();
    NodeContainer iotNodes;
    iotNodes.Create(numIoTDevices);

    NodeContainer allNodes;
    allNodes.Add(serverNode);
    allNodes.Add(iotNodes);

    // 3. Configure Mobility: Set fixed positions for all nodes
    MobilityHelper mobility;
    mobility.SetMobilityModelType("ns3::ConstantPositionMobilityModel");

    // Place the server at the origin (0,0,0)
    Ptr<ConstantPositionMobilityModel> serverMobility = CreateObject<ConstantPositionMobilityModel>();
    serverMobility->SetPosition(Vector(0.0, 0.0, 0.0));
    serverNode->AggregateObject(serverMobility);

    // Place IoT devices in a line, ensuring they are within range of the server
    // Default 802.15.4 range is typically tens of meters
    for (uint32_t i = 0; i < numIoTDevices; ++i)
    {
        Ptr<ConstantPositionMobilityModel> iotMobility = CreateObject<ConstantPositionMobilityModel>();
        iotMobility->SetPosition(Vector((i + 1) * 10.0, 0.0, 0.0)); // 10m spacing
        iotNodes.Get(i)->AggregateObject(iotMobility);
    }

    // 4. Install IEEE 802.15.4 NetDevices on all nodes
    Dot15D4Helper dot15d4Helper;
    NetDeviceContainer dot15d4Devices = dot15d4Helper.Install(allNodes);

    // 5. Install 6LoWPAN adaptation layer over 802.15.4 devices
    SixLowPanHelper sixLowPanHelper;
    NetDeviceContainer sixlowpanDevices = sixLowPanHelper.Install(dot15d4Devices);

    // 6. Install IPv6 stack on all nodes
    InternetStackHelper internetv6;
    internetv6.Install(allNodes);

    // 7. Assign IPv6 Addresses to 6LoWPAN interfaces
    Ipv6AddressHelper ipv6;
    ipv6.SetBase("2001:db8::", Ipv6Prefix(64)); // Use a common IPv6 prefix
    Ipv6InterfaceContainer ipv6Interfaces = ipv6.Assign(sixlowpanDevices);

    // Retrieve the server's global IPv6 address for client applications
    // The server node is the first in 'allNodes', hence its device is at index 0 in 'sixlowpanDevices'
    // and its interface is at index 0 in 'ipv6Interfaces'. Address index 1 is the global address.
    Ipv6Address serverIpv6Address = ipv6Interfaces.GetAddress(0, 1);

    // 8. Configure and Install Applications
    // Server Application: UDP Server on the central server node
    UdpServerHelper serverApp(serverPort);
    ApplicationContainer serverApps = serverApp.Install(serverNode);
    serverApps.Start(Seconds(1.0));             // Start server after 1 second
    serverApps.Stop(Seconds(simTime - 1.0)); // Stop server before simulation ends

    // Client Applications: UDP Clients on IoT devices
    // Each IoT device sends UDP packets periodically to the server
    UdpClientHelper clientApp(serverIpv6Address, serverPort);
    clientApp.SetAttribute("MaxPackets", UintegerValue(1000)); // Max packets to send per client
    clientApp.SetAttribute("Interval", TimeValue(Seconds(1.0))); // Send one packet every second
    clientApp.SetAttribute("PacketSize", UintegerValue(100));   // Packet size in bytes

    ApplicationContainer clientApps = clientApp.Install(iotNodes);
    clientApps.Start(Seconds(2.0));             // Start clients after server is up
    clientApps.Stop(Seconds(simTime - 0.5)); // Stop clients before simulation ends

    // 9. Enable NetAnim visualization (optional)
    NetAnimHelper netanim;
    netanim.TracePhy("iot-6lowpan-netanim.xml"); // Output animation XML file

    // 10. Configure and Run Simulation
    Simulator::Stop(Seconds(simTime)); // Set total simulation time
    Simulator::Run();                  // Start the simulation

    // 11. Clean up
    Simulator::Destroy();

    return 0;
}