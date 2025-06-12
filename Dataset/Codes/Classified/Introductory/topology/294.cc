#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMeshUdpExample");

int main(int argc, char *argv[])
{
    // Set default values for command line arguments
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("WifiMeshUdpExample", LOG_LEVEL_INFO);

    // Create nodes for the Wi-Fi mesh network
    NodeContainer wifiNodes;
    wifiNodes.Create(5); // Create 5 Wi-Fi nodes

    // Set up Wi-Fi Mesh Helper
    WifiMeshHelper wifiMesh;
    wifiMesh.SetRemoteStationManager("ns3::AarfWifiManager");

    // Set up Wi-Fi MAC and PHY attributes
    MeshHelper meshHelper;
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns3-mesh-ssid");
    wifiMac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer meshDevices = wifiMesh.Install(wifiPhy, wifiMac, wifiNodes);

    // Set up mobility model for the nodes
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel");
    mobility.Install(wifiNodes);

    // Install Internet stack on the nodes
    InternetStackHelper internet;
    internet.Install(wifiNodes);

    // Assign IP addresses to the nodes
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(meshDevices);

    // Set up the UDP server on the last node (destination)
    uint16_t port = 8080;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(wifiNodes.Get(4)); // Last node
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP client on the first node (source)
    UdpClientHelper udpClient(interfaces.GetAddress(4), port); // Sending to last node
    udpClient.SetAttribute("MaxPackets", UintegerValue(100));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = udpClient.Install(wifiNodes.Get(0)); // First node
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
