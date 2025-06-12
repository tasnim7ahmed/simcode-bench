#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MeshSimulation");

int main(int argc, char *argv[]) {
    uint32_t numMeshNodes = 4;
    bool pcap = false;
    uint16_t port = 9;
    uint32_t packetSize = 1024;
    uint32_t packetsPerClient = 1000;
    double interPacketInterval = 0.01;
    double simulationTime = 10.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("pcap", "Enable PCAP tracing", pcap);
    cmd.Parse(argc, argv);

    NodeContainer meshNodes;
    meshNodes.Create(numMeshNodes);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    MeshHelper mesh = MeshHelper::Default();
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.SetMacType("RngReq", BooleanValue(false));
    NetDeviceContainer meshDevices = mesh.Install(wifiPhy, meshNodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(meshNodes);

    InternetStackHelper internet;
    internet.Install(meshNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(meshDevices);

    if (pcap) {
        wifiPhy.EnablePcapAll("mesh_simulation");
    }

    // Server node: last node
    uint32_t serverNodeIndex = numMeshNodes - 1;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(meshNodes.Get(serverNodeIndex));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));

    // First three nodes are clients
    for (uint32_t i = 0; i < numMeshNodes - 1; ++i) {
        UdpClientHelper client(interfaces.GetAddress(serverNodeIndex), port);
        client.SetAttribute("MaxPackets", UintegerValue(packetsPerClient));
        client.SetAttribute("Interval", TimeValue(Seconds(interPacketInterval)));
        client.SetAttribute("PacketSize", UintegerValue(packetSize));

        ApplicationContainer clientApps = client.Install(meshNodes.Get(i));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(simulationTime));
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}