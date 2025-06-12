#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mesh-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging if needed
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    uint32_t numNodes = 4;
    double simTime = 10.0;

    CommandLine cmd;
    cmd.Parse(argc, argv);

    // 1. Create nodes
    NodeContainer meshNodes;
    meshNodes.Create(numNodes);

    // 2. Setup Wi-Fi PHY and channel
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    // 3. Setup mesh
    MeshHelper mesh;
    std::string stack = "ns3::Dot11sStack";
    mesh = MeshHelper::Default();
    mesh.SetStackInstaller(stack, "Root", Mac48AddressValue (Mac48Address ("00:00:00:00:00:01")));
    mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS);
    mesh.SetMacType ("RandomStart", TimeValue (Seconds (0.1)));

    NetDeviceContainer meshDevices = mesh.Install(phy, meshNodes);

    // 4. Mobility - Put nodes in a line with 50 m separation
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue (0.0),
                                  "MinY", DoubleValue (0.0),
                                  "DeltaX", DoubleValue (50.0),
                                  "DeltaY", DoubleValue (0.0),
                                  "GridWidth", UintegerValue (numNodes),
                                  "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install(meshNodes);

    // 5. Internet stack
    InternetStackHelper stackHelper;
    stackHelper.Install(meshNodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(meshDevices);

    // 6. UDP server on the last node
    uint16_t serverPort = 9;
    UdpServerHelper udpServer(serverPort);
    ApplicationContainer serverApp = udpServer.Install(meshNodes.Get(numNodes-1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simTime));

    // 7. UDP clients on the first three nodes
    uint32_t maxPackets = 1000;
    uint32_t packetSize = 1024;
    double interval = 0.01; // seconds

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numNodes - 1; ++i)
    {
        UdpClientHelper udpClient(interfaces.GetAddress(numNodes-1), serverPort);
        udpClient.SetAttribute("MaxPackets", UintegerValue(maxPackets));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
        udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));
        ApplicationContainer clientApp = udpClient.Install(meshNodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(simTime));
        clientApps.Add(clientApp);
    }

    // 8. Enable pcap tracing (optional)
    phy.EnablePcapAll("mesh-wifi");

    // 9. Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}