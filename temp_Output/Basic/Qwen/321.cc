#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MeshUdpSimulation");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 4;
    uint16_t port = 9;
    uint32_t packetSize = 1024;
    uint32_t numPackets = 1000;
    Time interPacketInterval = Seconds(0.01);
    Time simulationTime = Seconds(10.0);

    NodeContainer nodes;
    nodes.Create(numNodes);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    WifiHelper wifi = WifiHelper::Default();
    wifi.SetStandard(WIFI_STANDARD_80211s);

    MeshPointDeviceHelper meshHelper;
    meshHelper.SetStackInstaller("ns3::Dot11sStack");
    NetDeviceContainer meshDevices = meshHelper.Install(wifiPhy, wifi, nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(meshDevices);

    // Set up UDP server on the last node
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(numNodes - 1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(simulationTime);

    // Set up UDP clients on first three nodes
    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        UdpClientHelper client(interfaces.GetAddress(numNodes - 1), port);
        client.SetAttribute("MaxPackets", UintegerValue(numPackets));
        client.SetAttribute("Interval", TimeValue(interPacketInterval));
        client.SetAttribute("PacketSize", UintegerValue(packetSize));

        ApplicationContainer clientApps = client.Install(nodes.Get(i));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(simulationTime);
    }

    Simulator::Stop(simulationTime);
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}