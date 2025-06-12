#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mesh-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMeshUdpTest");

int main(int argc, char *argv[])
{
    uint32_t numNodes = 4;
    uint32_t packetCount = 1000;
    Time interPacketInterval = Seconds(0.01);
    uint16_t port = 9;
    double simulationTime = 10.0;

    NodeContainer nodes;
    nodes.Create(numNodes);

    YansWifiPhyHelper wifiPhy;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211s);

    MeshHelper mesh = MeshHelper::Default();
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.SetMacType("RngReq", BooleanValue(false));

    NetDeviceContainer devices = mesh.Install(wifiPhy, nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(numNodes - 1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));

    UdpClientHelper client(interfaces.GetAddress(numNodes - 1), port);
    client.SetAttribute("MaxPackets", UintegerValue(packetCount));
    client.SetAttribute("Interval", TimeValue(interPacketInterval));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numNodes - 1; ++i)
    {
        clientApps.Add(client.Install(nodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}