#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/propagation-module.h"
#include "ns3/energy-module.h"
#include "ns3/devices-module.h"
#include "ns3/simple-device-energy-model.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ZigbeeWSN");

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponent::SetFilter("ZigbeeWSN", LOG_LEVEL_INFO);

    uint32_t numNodes = 10;
    double simulationTime = 10.0;
    double packetInterval = 0.5; //seconds
    uint32_t packetSize = 128;

    NodeContainer nodes;
    nodes.Create(numNodes);

    // Mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(0, 0, 100, 100)));
    mobility.Install(nodes);

    // Install ZigBee devices
    ZigBeeHelper zigBeeHelper;
    NetDeviceContainer devices = zigBeeHelper.Install(nodes);

    // Set up the network
    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Coordinator role
    zigBeeHelper.AssignRoles(devices.Get(0), ZigBeeRole::COORDINATOR);
    for (uint32_t i = 1; i < numNodes; ++i) {
        zigBeeHelper.AssignRoles(devices.Get(i), ZigBeeRole::END_DEVICE);
    }

    // UDP client application
    UdpClientHelper client(interfaces.GetAddress(0), 9); // Coordinator address, port 9
    client.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < numNodes; ++i) {
        clientApps.Add(client.Install(nodes.Get(i)));
    }

    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime));

    // UDP echo server on the coordinator
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime + 1));

    // PCAP tracing
    Packet::EnablePcapAll("zigbee_wsn");

    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}