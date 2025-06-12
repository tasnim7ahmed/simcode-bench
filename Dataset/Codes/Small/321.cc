#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/mesh-module.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleMeshExample");

int main(int argc, char *argv[])
{
    // Enable log components
    LogComponentEnable("SimpleMeshExample", LOG_LEVEL_INFO);

    // Create mesh nodes
    NodeContainer meshNodes;
    meshNodes.Create(4);

    // Set up Wi-Fi mesh network
    MeshHelper meshHelper;
    meshHelper.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);
    meshHelper.SetRemoteStationManager("ns3::ArfWifiManager");

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns-3-mesh-ssid");
    wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    WifiMacHelper apMac;
    apMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

    // Install Wi-Fi devices (mesh nodes)
    NetDeviceContainer meshDevices = meshHelper.Install(wifiPhy, wifiMac, meshNodes);

    // Install internet stack (IP, UDP, etc.)
    InternetStackHelper stack;
    stack.Install(meshNodes);

    // Assign IP addresses to mesh nodes
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer meshIpIface = ipv4.Assign(meshDevices);

    // Set up UDP server on the last mesh node
    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(meshNodes.Get(3));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP client on each of the other mesh nodes
    UdpClientHelper udpClient(meshIpIface.GetAddress(3), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp;
    for (uint32_t i = 0; i < 3; ++i)
    {
        clientApp.Add(udpClient.Install(meshNodes.Get(i)));
    }
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
